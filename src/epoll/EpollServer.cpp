#include "EpollServer.hpp"
#include "HttpParser.hpp"
#include <fstream>

EpollServer::EpollServer() : _epollFd(-1)
{
    _epollFd = epoll_create1(0);
    if (_epollFd == -1)
        throw std::runtime_error("epoll_create1 failed");
}

EpollServer::~EpollServer()
{
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

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        close(fd);
        throw std::runtime_error("bind failed");
    }
    if (listen(fd, SOMAXCONN) == -1)
    {
        close(fd);
        throw std::runtime_error("listen failed");
    }
    return fd;
}

void EpollServer::addServer(ServerConfig &config)
{
    int fd = _createAndBindSocket(config.getHost(), config.getPort());
    _setNonBlocking(fd);
    _registerToEpoll(fd, EPOLLIN);
    _listenFds.insert(fd);
    _fdToConfig[fd] = &config;

    std::ostringstream oss;
    oss << config.getPort();
    utils::log_info("Listening on " + config.getHost() + ":" + oss.str());
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

void EpollServer::_handleClientData(int fd)
{
    char buffer[4096];
    ClientData &data = _clients[fd];

    ssize_t bytesRead = recv(fd, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0)
    {
        // Client disconnected — but drain send_buf first if we have a response
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
    bool complete = data.parser.feed(newData);

    _createResponse(fd, complete, data);
}

void EpollServer::_createResponse(int fd, bool complete, ClientData &data)
{
    if (complete)
    {
        HttpRequest request = data.parser.getRequest();

        std::string path = request.getPath();
        if (path == "/")
            path = "/index.html";

        std::string body;
        std::string contentType = "text/plain";

        std::string filePath = "www" + path;
        std::ifstream file(filePath.c_str(), std::ios::binary);
        if (file.good())
        {
            std::ostringstream content;
            content << file.rdbuf();
            body = content.str();
            file.close();

            if (path.find(".bin") != std::string::npos)
                contentType = "application/octet-stream";
            else if (path.find(".html") != std::string::npos)
                contentType = "text/html";
            else if (path.find(".css") != std::string::npos)
                contentType = "text/css";
            else if (path.find(".js") != std::string::npos)
                contentType = "application/javascript";
        }
        else
        {
            body = "Request received successfully.\nPath: " + request.getPath();
            if (!request.getBody().empty())
                body += "\nBody: " + request.getBody();
        }

        std::ostringstream oss;
        oss << request.getVersion() << " " << request.getErrorCode() << " OK\r\n"
            << "Content-Type: " << contentType << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n";

        if (request.getMethod() != METHOD_HEAD)
            oss << body;

        data.send_buf = oss.str();

        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
        ev.data.fd = fd;
        epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev);
    }
    else if (data.parser.getState() == PARSE_ERROR)
    {
        HttpRequest request = data.parser.getRequest();

        std::string body = "Bad Request";
        std::ostringstream oss;
        oss << request.getVersion() << " " << request.getErrorCode() << " OK\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << body;
        data.send_buf = oss.str();

        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
        ev.data.fd = fd;
        epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev);
    }
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
    {
        _closeClient(fd);
        return;
    }

    ssize_t sent = send(fd, data.send_buf.data(), data.send_buf.size(), 0);
    if (sent <= 0)
    {
        _closeClient(fd);
        return;
    }

    data.send_buf.erase(0, sent);
    data.last_activity = time(NULL);

    if (data.send_buf.empty())
        _closeClient(fd);
}

void EpollServer::run()
{
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
