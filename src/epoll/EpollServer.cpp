#include "EpollServer.hpp"

EpollServer::EpollServer() : _epollFd(-1)
{
    _epollFd = epoll_create1(0);
    if (_epollFd == -1)
        throw std::runtime_error("epoll_create1 failed");
}

EpollServer::~EpollServer()
{
    for (std::map<int, EpollClient *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        close(it->first);
        delete it->second;
    }
    for (std::set<int>::iterator it = _listenFds.begin(); it != _listenFds.end(); ++it)
        close(*it);
    if (_epollFd != -1)
        close(_epollFd);
}

void EpollServer::_setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl getfl failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl setfl failed");
}

void EpollServer::_registerToEpoll(int fd, uint32_t events)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1)
        throw std::runtime_error("epoll_ctl ADD failed");
}

void EpollServer::_verifyGetAddr(int ret, int fd)
{
    if (ret != 0)
    {
        close(fd);
        throw std::runtime_error(std::string("getaddrinfo failed "));
    }
}

void EpollServer::_verifyBind(int fd, struct addrinfo *res, std::ostringstream *oss, const std::string &host)
{
    if (bind(fd, res->ai_addr, res->ai_addrlen) == -1)
    {
        freeaddrinfo(res);
        close(fd);
        if (errno == EADDRINUSE)
            throw std::runtime_error("bind failed: EADDRINUSE on " + host + ":" + oss->str());
        throw std::runtime_error("bind failed");
    }
}

void EpollServer::_verifyListen(int fd)
{
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

// ─── Public API ──────────────────────────────────────────────────────────────

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
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(listenFd, (struct sockaddr *)&clientAddr, &clientLen);

        if (clientFd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            utils::log_error("accept() failed");
            break;
        }
        _setNonBlocking(clientFd);
        _registerToEpoll(clientFd, EPOLLIN | EPOLLERR | EPOLLHUP);
        _clients[clientFd] = new EpollClient(clientFd, _epollFd, _fdToConfig[listenFd]);
        std::cout << "New Client fd=" << clientFd << std::endl;
    }
}

void EpollServer::_closeClient(int fd)
{
    if (_clients.count(fd) == 0)
        return;
    epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    delete _clients[fd];
    _clients.erase(fd);
    std::cout << "Connection closed on fd " << fd << std::endl;
}

void EpollServer::_checkTimeout()
{
    time_t now = time(NULL);
    std::vector<int> timedOut;

    for (std::map<int, EpollClient *>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        if (it->second->isTimedOut(now))
            timedOut.push_back(it->first);

    for (std::vector<int>::iterator it = timedOut.begin(); it != timedOut.end(); ++it)
    {
        utils::log_info("Client timed out, closing fd");
        _closeClient(*it);
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

            if (_listenFds.count(fd))
            {
                _acceptNewClient(fd);
            }
            else if (_clients.count(fd))
            {
                if (ev & (EPOLLERR | EPOLLHUP))
                    _closeClient(fd);
                else
                {
                    bool shouldClose = false;
                    if (ev & EPOLLIN)
                        shouldClose = _clients[fd]->handleRead();
                    if (!shouldClose && _clients.count(fd) && (ev & EPOLLOUT))
                        shouldClose = _clients[fd]->handleWrite();
                    if (shouldClose)
                        _closeClient(fd);
                }
            }
        }
    }
}
