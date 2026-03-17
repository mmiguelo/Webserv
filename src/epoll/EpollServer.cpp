#include "EpollServer.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpRouter.hpp"
#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

EpollServer::EpollServer() : _epollFd(-1)
{
    _epollFd = epoll_create1(0);
    if (_epollFd == -1)
        throw std::runtime_error("epoll_create1 failed");
}

EpollServer::~EpollServer()
{
    for (std::map<int, ClientData>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        close(it->first);
    for (std::set<int>::iterator it = _listenFds.begin(); it != _listenFds.end(); ++it)
        close(*it);
    if (_epollFd != -1)
        close(_epollFd);
}

void EpollServer::_setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // get the binary flag for fd
    if (flags == -1)
        throw std::runtime_error("fcntl getfl failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) // set the flag to nonblocking
        throw std::runtime_error("fcntl setfl failed");
}

void EpollServer::_registerToEpoll(int fd, uint32_t events)
{
    struct epoll_event epoll_ev;
    epoll_ev.events = events;
    epoll_ev.data.fd = fd;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &epoll_ev) == -1) // add the new fd to the list
        throw std::runtime_error("epoll ctl failed");
}

void EpollServer::_verifyGetAddr(int ret, int fd) {
    if (ret != 0)
    {
        close(fd);
        throw std::runtime_error(std::string("getaddrinfo failed: ") + gai_strerror(ret));
    }
}

void EpollServer::_verifyBind(int fd, struct addrinfo *res, std::ostringstream *oss, const std::string &host) {
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1)
    {
        freeaddrinfo(res);
        close(fd);
        if (errno == EADDRINUSE)
            throw std::runtime_error("bind failed: EADDRINUSE on " + host + ":" + oss->str());
        throw std::runtime_error("bind failed");
    }
}

void EpollServer::_verifyListen(int fd) {
    if (listen(fd, SOMAXCONN) == -1)
    {
        close(fd);
        throw std::runtime_error("listen failed");
    }
}


int EpollServer::_createAndBindSocket(const std::string &host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(fd);
        throw std::runtime_error("setsockopt failed");
    }

    std::ostringstream oss;
    oss << port;

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = NULL;
    const char *node = host.empty() ? NULL : host.c_str();

    int ret = getaddrinfo(node, oss.str().c_str(), &hints, &res);

    _verifyGetAddr(ret, fd);
    _verifyBind(fd, res, &oss, host);
    freeaddrinfo(res);
    _verifyListen(fd);
    return fd;
}

void EpollServer::addServer(ServerConfig *config, int port)
{
    int fd = _createAndBindSocket(config->getListenDirectives()[0].host, port);
    _setNonBlocking(fd);
    _registerToEpoll(fd, EPOLLIN);
    _listenFds.insert(fd);
    _fdToConfig[fd] = config;

    std::ostringstream oss;
    oss << port;
    utils::log_info("Listening on http://" + config->getListenDirectives()[0].host + ":" + oss.str());
}

void EpollServer::_acceptNewClient(int listenFd)
{
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listenFd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            utils::log_error("accept() failed");
            break;
        }
        _setNonBlocking(client_fd);
        _registerToEpoll(client_fd, EPOLLIN | EPOLLERR | EPOLLHUP);
        ClientData data;
        data.last_activity = time(NULL);
        data.server_fd = listenFd;
        data.server_config = _fdToConfig[listenFd];
        data.continue_sent = false;
        data.should_close_after_send = false;
        _clients[client_fd] = data;
        std::cout << "New Client fd = " << client_fd << std::endl;
    }
}

void EpollServer::_checkTimeout()
{
    time_t now = time(NULL);
    std::vector<int> fdTimeout;

    for (std::map<int, ClientData>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (now - it->second.last_activity > MAX_TIMEOUT)
            fdTimeout.push_back(it->first);
    }

    for (size_t i = 0; i < fdTimeout.size(); ++i)
    {
        utils::log_info("Client timed out, closing fd");
        _closeClient(fdTimeout[i]);
    }
}

void EpollServer::_processPipelines(int fd, ClientData *data) {
    while (true) {
        ParserState st = data->parser.getState();
        HttpRequest &request = data->parser.getRequest();

        if (st == PARSE_ERROR) {
            // Build error response and close after send
            _createResponse(fd, true, data);
            break;
        }

        if (st == PARSE_EXPECT_CONTINUE) {
            if (!data->continue_sent) {
                std::string continueResponse = request.getVersion() + " 100 Continue\r\n\r\n";
                send(fd, continueResponse.c_str(), continueResponse.size(), 0);
                data->continue_sent = true;
            }
            // Wait for the body to arrive in subsequent recv
            break;
        }

        if (st == PARSE_COMPLETE) {
            // Build response for the completed request and append it to send_buf
            _createResponse(fd, true, data);

            // Save remaining bytes, reset parser, and continue parsing leftover
            std::string leftover = data->parser.takeBuffer();
            data->parser.reset();
            data->continue_sent = false; // reset continue flag between requests
            if (leftover.empty())
                break;
            // Feed leftover into parser and loop to attempt to parse next request
            data->parser.feed(leftover, *data->server_config);
            continue;
        }

        // Otherwise (need more data), stop processing
        break;
    }
}

void EpollServer::_handleClientData(int fd)
{
    char buffer[4096];
    ClientData &data = _clients[fd];

    ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0)
    {
        if (!data.send_buf.empty())
        {
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
            ev.data.fd = fd;
            epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev);
            return;
        }
        _closeClient(fd);
        return;
    }

    data.last_activity = time(NULL);

    std::string newData(buffer, bytesRead);
    // Feed new bytes into parser
    data.parser.feed(newData, *data.server_config);

    // Process as many complete requests as available in the parser buffer
    _processPipelines(fd, &data);
    
}

bool EpollServer::_keepAlive(const HttpRequest &request)
{
    std::string connHeader;
    if (request.hasHeader("connection"))
        connHeader = toLowerStr(request.getHeader("connection"));
    
        std::string version = request.getVersion().empty() ? "HTTP/1.1" : request.getVersion();

        if (version == "HTTP/1.1") return connHeader != "close";
        if (version == "HTTP/1.0") return connHeader == "keep-alive";
    return false;
}

std::string EpollServer::_buildResponse(ClientData *data, const HttpRequest &request, int statusCode)
{
    HttpResponse response;
    ServerConfig* config = _fdToConfig[data->server_fd];
    HttpRouter router;
    HttpRouteMatch match = router.route(request, *config);

    if (match.errorCode == 301 || match.errorCode == 302) {
        response.build(match.errorCode, "", "", request.getVersion());
        response.setLocation(match.redirectTarget);
        return response.serialize(request.getMethod());
    }
    if (match.errorCode == 405) {
        data->should_close_after_send = true;
        response.build(405, "", "", request.getVersion());
        response.setAllow(match.location->methods);
        return response.serialize(request.getMethod());
    }
    if (match.errorCode != 0)
        return response.buildError(statusCode, request);
    if (match.executeCGI)
        return response.buildError(501, request); // TODO: CGI handler

    std::string result = response.buildFromFile(request, match.path);
    if (response.getStatusCode() >= 400)
        data->should_close_after_send = true;
    return result;
}

void EpollServer::_createResponse(int fd, bool complete, ClientData *data)
{
    HttpResponse response;
    HttpRequest request = data->parser.getRequest();
    std::string responseStr;
    int statusCode = static_cast<int>(request.getErrorCode());
    // Decide keep-alive based on request version and Connection header
    bool keepAlive = _keepAlive(request);

    // default: if parser error or incomplete, we still want to send error and close
    if (data->parser.getState() == PARSE_ERROR)
    {
        responseStr = response.buildError(statusCode, request);
        data->should_close_after_send = true;
    }
    else if (!complete)
    {
        responseStr = response.buildError(400, request); // Incomplete request, treat as bad request
        data->should_close_after_send = true;
    }
    else if (statusCode >= 400)
    {
        responseStr = response.buildError(statusCode, request);
        data->should_close_after_send = true;
    }
    else
    {
        
    }

    // Set Connection header on response according to keepAlive decision and server desire to close
    if (data->should_close_after_send)
        response.setConnection("close");
    else
        response.setConnection(keepAlive ? "keep-alive" : "close");

    if (responseStr.empty())
        responseStr = response.serialize(request.getMethod());
    // Append response so multiple pipelined responses are sent in order
    data->send_buf += responseStr;

    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    ev.data.fd = fd;
    epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev);
}

void EpollServer::_closeClient(int fd)
{
    // Guard against double-close
    if (_clients.count(fd) == 0)
        return;
    epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    _clients.erase(fd);
    std::cout << "Connection closed on fd " << fd << std::endl;
}

void EpollServer::_handleClientResponse(int fd)
{
    ClientData &data = _clients[fd];

    if (data.send_buf.empty())
        return;

    std::cout << "[sending] fd=" << fd << " pending_bytes=" << data.send_buf.size() << std::endl;
    ssize_t sent = send(fd, data.send_buf.data(), data.send_buf.size(), 0);
    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return; // send buffer full, wait for next EPOLLOUT
        std::cerr << "[send error] fd=" << fd << " errno=" << errno << " err=" << strerror(errno) << std::endl;
        _closeClient(fd);
        return;
    }
    if (sent == 0)
    {
        _closeClient(fd);
        return;
    }

    data.send_buf.erase(0, sent);
    data.last_activity = time(NULL);
    // If we've sent the full response, decide whether to close or to keep-alive
    if (data.send_buf.empty()) {
        if (data.should_close_after_send) {
            _closeClient(fd);
            return;
        }
        // Keep-alive: reset parser state to accept a new request on same connection
        data.parser.reset();
        data.continue_sent = false;
        data.should_close_after_send = false;

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        ev.data.fd = fd;
        epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev);
    }
}

void EpollServer::run()
{
    std::cout << _fdToConfig.size() << " server(s) configured, waiting for connections..." << std::endl;
    while (true)
    {
        int n = epoll_wait(_epollFd, _events, MAX_EVENTS, 1000);

        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("epoll_wait failed");
        }

        _checkTimeout();
        for (int i = 0; i < n; i++)
        {
            int fd = _events[i].data.fd;
            uint32_t ev = _events[i].events;

            if (_listenFds.count(fd)) // it's a listen socket → accept
            {
                std::cout << _listenFds.size() << " listen sockets, accepting on fd " << fd << std::endl;
                _acceptNewClient(fd);
            }
            else if (_clients.count(fd)) // it's a client socket → handle
            {
                if (ev & (EPOLLERR | EPOLLHUP))
                    _closeClient(fd);
                else
                {
                    if (ev & EPOLLIN)
                        _handleClientData(fd);
                    if (_clients.count(fd) && (ev & EPOLLOUT))

                        _handleClientResponse(fd);
                }
            }
        }
    }
}
