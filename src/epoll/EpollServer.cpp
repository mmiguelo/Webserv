#include "EpollServer.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <fstream>
#include <sstream>

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
    std::string ip = host;
    if (ip == "localhost")
        ip = "127.0.0.1";
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

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

void EpollServer::addServer(ServerConfig &config, int port)
{
    int fd = _createAndBindSocket(config.getHost(), port);
    _setNonBlocking(fd);
    _registerToEpoll(fd, EPOLLIN);
    _listenFds.insert(fd);
    _fdToConfig[fd] = &config;

    std::ostringstream oss;
    oss << port;
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
        data.continue_sent = false;  // Add this
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
    bool complete = data.parser.feed(newData, *_fdToConfig[data.server_fd]);
    HttpRequest& request = data.parser.getRequest();

    // Handle Expect: 100-continue
    if (data.parser.getState() == PARSE_ERROR)
    {
        // Error detected (including 413 Payload Too Large) - respond immediately
        _createResponse(fd, complete, data);
    }
    else if (!data.continue_sent && request.hasHeader("expect"))
    {
        std::string expect = request.getHeader("expect");
        if (expect.find("100-continue") != std::string::npos)
        {
            // Send 100 Continue to allow client to send body
            std::string continueResponse = request.getVersion() + " 100 Continue\r\n\r\n";
            send(fd, continueResponse.c_str(), continueResponse.size(), 0);
            data.continue_sent = true;
            return; // Wait for body data
        }
    }
    else if (complete)
    {
        _createResponse(fd, complete, data);
    }
    // Otherwise, wait for more data
}

void EpollServer::_createResponse(int fd, bool complete, ClientData &data)
{
    HttpResponse response;
    HttpRequest request = data.parser.getRequest();
    std::string responseStr;
    int statusCode = static_cast<int>(request.getErrorCode());

    if (data.parser.getState() == PARSE_ERROR)
        responseStr = response.buildError(statusCode, request);
    else if (complete) {
        if (statusCode >= 400)
            responseStr = response.buildError(statusCode, request);
        else {
            // Get the root directory from config
            ServerConfig* config = _fdToConfig[data.server_fd];
            std::string root = config->getRoot();
                        
            // Let HttpResponse handle file serving and 404
            responseStr = response.buildFromFile(request, root);
        }
    }
    else
        responseStr = response.buildError(400, request); // Incomplete request, treat as bad request
    
    if (responseStr.empty())
        responseStr = response.serialize(request.getMethod());
    data.send_buf = responseStr;

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

                        _handleClientResponse(fd);                }
            }
        }
    }
}
