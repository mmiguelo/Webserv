#include "EpollServer.hpp"

EpollServer::EpollServer(const std::string &host, int port) : _listenFd(-1), _epollFd(-1), _port(port), _host(host) {}
EpollServer::~EpollServer() {}

void EpollServer::_createSocket()
{
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd == -1)
        throw std::runtime_error("socket failed");

    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("setsockopt failed");
}

void EpollServer::_setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl getfl failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl setfl failed");
}

void EpollServer::_bindAndListen()
{
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = inet_addr(_host.c_str());

    if (bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        throw std::runtime_error("bind failed");

    if (listen(_listenFd, SOMAXCONN) == -1)
        throw std::runtime_error("listen failed");
}

void EpollServer::_registerToEpoll(int fd, uint32_t events)
{
    struct epoll_event epoll_ev;
    epoll_ev.events = events;
    epoll_ev.data.fd = fd;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &epoll_ev) == -1)
        throw std::runtime_error("epoll ctl failed");
}

void EpollServer::_acceptNewClient()
{
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(_listenFd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            utils::log_error("accept() failed");
            break;
        }

        _setNonBlocking(client_fd);

        std::cout << "New Client fd = " << client_fd << std::endl;
        close(client_fd);
    }
}

void EpollServer::init()
{
    _epollFd = epoll_create1(0);
    _createSocket();
    _setNonBlocking(_listenFd);
    _bindAndListen();
    _registerToEpoll(_listenFd, EPOLLIN);

    utils::log_info("Server listening");
}

void EpollServer::run()
{
    while (true)
    {
        int n = epoll_wait(_epollFd, _events, MAX_EVENTS, -1);

        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("epoll did not waited ahahahah");
        }

        for (int i = 0; i < n; i++)
        {
            int fd = _events[i].data.fd;

            if (fd == _listenFd)
                _acceptNewClient();
        }
    }
}
