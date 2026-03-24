#include "EpollClient.hpp"
#include "EpollServer.hpp"
#include "utils.hpp"

EpollClient::EpollClient(int fd, int epollFd, ServerConfig *config, EpollServer *server)
    : _fd(fd), _epollFd(epollFd), _config(config), _server(server),
      _lastActivity(time(NULL)), _continueSent(false), _closeAfterSend(false),
      _cgi_pid(-1), _cgi_stdin_fd(-1), _cgi_stdout_fd(-1), _cgi_input_offset(0), _cgi_start_time(0), _cgi_finished(false)
{
}

int EpollClient::getFd() const
{
    return _fd;
}

bool EpollClient::isTimedOut(time_t now) const
{
    return (now - _lastActivity) > MAX_TIMEOUT;
}

// Returns true → server must close this client
bool EpollClient::handleRead()
{
    char buffer[4096];
    ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), 0);

    if (bytesRead <= 0)
    {
        if (!_sendBuffer.empty())
        {
            _switchToWrite();
            return false; // still have data to send, don't close yet
        }
        return true; // signal server to close
    }

    _lastActivity = time(NULL);
    _parser.feed(std::string(buffer, bytesRead), *_config);
    _processPipelines();
    return false;
}

// Returns true → server must close this client
bool EpollClient::handleWrite()
{
    if (_sendBuffer.empty())
        return false;

    ssize_t sent = send(_fd, _sendBuffer.data(), _sendBuffer.size(), 0);
    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;
        return true;
    }
    if (sent == 0)
        return true;

    _sendBuffer.erase(0, sent);
    _lastActivity = time(NULL);

    if (_sendBuffer.empty())
    {
        if (_closeAfterSend)
            return true;

        _parser.reset();
        _continueSent = false;
        _closeAfterSend = false;
        _switchToRead();
    }
    return false;
}

void EpollClient::_switchToWrite()
{
    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    ev.data.fd = _fd;
    epoll_ctl(_epollFd, EPOLL_CTL_MOD, _fd, &ev);
}

void EpollClient::_switchToRead()
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.fd = _fd;
    epoll_ctl(_epollFd, EPOLL_CTL_MOD, _fd, &ev);
}

void EpollClient::_processPipelines()
{
    while (true)
    {
        ParserState st = _parser.getState();
        HttpRequest &request = _parser.getRequest();

        if (st == PARSE_ERROR)
        {
            _createResponse(true);
            break;
        }
        if (st == PARSE_EXPECT_CONTINUE)
        {
            if (!_continueSent)
            {
                std::string cont = request.getVersion() + " 100 Continue\r\n\r\n";
                send(_fd, cont.c_str(), cont.size(), 0);
                _continueSent = true;
            }
            break;
        }
        if (st == PARSE_COMPLETE)
        {
            _createResponse(true);
            std::string leftover = _parser.takeBuffer();
            _parser.reset();
            _continueSent = false;
            if (leftover.empty())
                break;
            _parser.feed(leftover, *_config);
            continue;
        }
        break;
    }
}

void EpollClient::_createResponse(bool complete)
{
    HttpResponse response;
    HttpRequest request = _parser.getRequest();
    std::string responseStr;

    _closeAfterSend = false;

    bool keepAlive = _keepAlive(request);

    if (!complete)
    {
        responseStr = response.buildError(400, request, *_config);
        _closeAfterSend = true;
    }
    else if (!_buildErrorResponse(request, response, responseStr))
    {
        _buildRoutedResponse(request, response, responseStr);
    }

    if (_cgi_pid != -1)
        return;

    _finalizeResponse(request, response, responseStr, keepAlive);
}

bool EpollClient::_keepAlive(const HttpRequest &request) const
{
    std::string connHeader;
    if (request.hasHeader("connection"))
        connHeader = toLowerStr(request.getHeader("connection"));

    std::string version = request.getVersion().empty() ? "HTTP/1.1" : request.getVersion();

    bool result = false;
    if (version == "HTTP/1.1")
        result = connHeader != "close";
    if (version == "HTTP/1.0")
        result = connHeader == "keep-alive";

    return result;
}

bool EpollClient::_buildErrorResponse(const HttpRequest &request, HttpResponse &response, std::string &responseStr)
{
    int statusCode = static_cast<int>(request.getErrorCode());

    if (_parser.getState() == PARSE_ERROR || statusCode >= 400)
    {
        responseStr = response.buildError(statusCode, request, *_config);
        _closeAfterSend = true;
        return true;
    }
    return false;
}

void EpollClient::_buildRoutedResponse(const HttpRequest &request, HttpResponse &response, std::string &responseStr)
{
    HttpRouter router;
    HttpRouteMatch match = router.route(request, *_config);

    if (match.errorCode == 301 || match.errorCode == 302)
    {
        response.build(match.errorCode, "", "", request.getVersion());
        response.setLocation(match.redirectTarget);
        responseStr = response.serialize(request.getMethod());
    }
    else if (match.errorCode == 405)
    {
        response.build(405, "", "", request.getVersion());
        response.setAllow(match.location->methods);
        responseStr = response.serialize(request.getMethod());
        _closeAfterSend = true;
    }
    else if (match.errorCode != 0)
    {
        responseStr = response.buildError(match.errorCode, request, *_config);
        _closeAfterSend = true;
    }
    else if (match.executeCGI)
    {
        responseStr = response.buildError(501, request, *_config);
    }
    else
    {
        if (request.getMethod() == METHOD_POST && match.location != NULL &&
            match.location->has_client_max_body_size &&
            request.getBody().size() > match.location->client_max_body_size)
        {
            responseStr = response.buildError(413, request, *_config);
            _closeAfterSend = true;
            return;
        }

        if (request.getMethod() == METHOD_POST && !match.upload_dir.empty())
        {
            responseStr = response.handleUpload(request, match.upload_dir, *_config);
            if (response.getStatusCode() >= 400)
                _closeAfterSend = true;
            return;
        }
        struct stat st;
        int result;
        if (stat(match.path.c_str(), &st) != 0)
            responseStr = response.buildError(404, request, *_config);
        else {
            result = response.checkFile(st);
            
            if (request.getMethod() == METHOD_DELETE)
                responseStr = response.handleDelete(request, match.path, result, *_config);
            else if (result == 300)
                responseStr = response.buildFromDirectory(request, match.path, match.autoindex, *_config);
            else
                responseStr = response.buildFromFile(request, match.path, result, *_config);
            if (response.getStatusCode() >= 400)
                _closeAfterSend = true;
        }
    }
}

void EpollClient::_finalizeResponse(const HttpRequest &request, HttpResponse &response, std::string &responseStr, bool keepAlive)
{
    if (_closeAfterSend)
        response.setConnection("close");
    else
        response.setConnection(keepAlive ? "keep-alive" : "close");

    if (responseStr.empty())
        responseStr = response.serialize(request.getMethod());

    _sendBuffer += responseStr;
    _switchToWrite();
}

pid_t EpollClient::getCgiPid() const {
    return _cgi_pid;
}
int EpollClient::getCgiStdinFd() const {
    return _cgi_stdin_fd;
}
int EpollClient::getCgiStdoutFd() const {
    return _cgi_stdout_fd;
}
std::string EpollClient::getCgiInputBuffer() const {
    return _cgi_input_buffer;
}
std::string EpollClient::getCgiOutputBuffer() const {
    return _cgi_output_buffer;
}
size_t EpollClient::getCgiInputOffset() const {
    return _cgi_input_offset;
}
time_t EpollClient::getCgiStartTime() const {
    return _cgi_start_time;
}

void EpollClient::setCgiInputOffset(size_t value) {
    _cgi_input_offset = value;
}

void EpollClient::setCgiStdinFd(int fd) {
    _cgi_stdin_fd = fd;
}

void EpollClient::setCgiStdoutFd(int fd) {
    _cgi_stdout_fd = fd;
}

void EpollClient::setCgiPid(int pid) {
    _cgi_pid = pid;
}

void EpollClient::setSendBuffer(const std::string &newBuffer) {
    _sendBuffer = newBuffer;
}

std::string EpollClient::getSendBuffer() const {
    return _sendBuffer;
}

void EpollClient::appendCgiStdoutBuffer(const std::string &other, size_t size) {
    _cgi_output_buffer += std::string(other.c_str(), size);
}

void EpollClient::setCgiDone(bool flag) {
    _cgi_finished = flag;
}

void EpollClient::startCgi(pid_t pid, int stdinFd, int stdoutFd, const std::string &body) {
    _cgi_pid = pid;
    _cgi_stdin_fd = stdinFd;
    _cgi_stdout_fd = stdoutFd;
    _cgi_input_buffer = body;
    _cgi_input_offset = 0;
    _cgi_output_buffer.clear();
    _cgi_start_time = time(NULL);
    _cgi_finished = false;

    _server->registerCgi(_fd, _cgi_stdin_fd, _cgi_stdout_fd);
}