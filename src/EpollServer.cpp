/* ************************************************************************** */
/*                                                                            */
/*   EpollServer.cpp                                                          */
/*                                                                            */
/*   Implementation of the epoll-based event loop.                            */
/*                                                                            */
/* ************************************************************************** */

#include "EpollServer.hpp"
#include "utils.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ────────────────────── Constructor / Destructor ────────────────────── */

EpollServer::EpollServer(const std::string &host, int port)
    : _host(host), _port(port), _listenFd(-1), _epfd(-1)
{
    std::memset(_events, 0, sizeof(_events));

    /* 1. Create epoll instance */
    _epfd = epoll_create1(0);
    if (_epfd == -1)
    {
        ws::log_error("epoll_create1 failed: " + std::string(std::strerror(errno)));
        std::exit(EXIT_FAILURE);
    }
    ws::log_info("epoll instance created (fd=" + ws::itos(_epfd) + ")");

    /* 2-6. Create listen socket, bind, listen, register with epoll */
    _listenFd = createListenSocket();
    addToEpoll(_listenFd, EPOLLIN);
    ws::log_info("Listening on " + _host + ":" + ws::itos(_port) + " (fd=" + ws::itos(_listenFd) + ")");
}

EpollServer::~EpollServer()
{
    /* Close every client fd */
    for (std::map<int, ClientState>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        close(it->first);
    }
    _clients.clear();

    if (_listenFd != -1)
        close(_listenFd);
    if (_epfd != -1)
        close(_epfd);

    ws::log_info("Server shut down");
}

/* ───────────────────────── Setup helpers ────────────────────────────── */

int EpollServer::createListenSocket()
{
    /* Create TCP socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        ws::log_error("socket() failed: " + std::string(std::strerror(errno)));
        std::exit(EXIT_FAILURE);
    }

    /* SO_REUSEADDR — must be before bind() */
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        ws::log_error("setsockopt(SO_REUSEADDR) failed: " + std::string(std::strerror(errno)));
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }

    /* Non-blocking */
    setNonBlocking(sockfd);

    /* Bind */
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) <= 0)
    {
        ws::log_error("Invalid host address: " + _host);
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }

    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1)
    {
        ws::log_error("bind() failed: " + std::string(std::strerror(errno)));
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }

    /* Listen — backlog of 128 pending connections */
    if (listen(sockfd, 128) == -1)
    {
        ws::log_error("listen() failed: " + std::string(std::strerror(errno)));
        close(sockfd);
        std::exit(EXIT_FAILURE);
    }

    return sockfd;
}

void EpollServer::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        ws::log_error("fcntl(F_GETFL) failed: " + std::string(std::strerror(errno)));
        std::exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        ws::log_error("fcntl(F_SETFL) failed: " + std::string(std::strerror(errno)));
        std::exit(EXIT_FAILURE);
    }
}

void EpollServer::addToEpoll(int fd, uint32_t events)
{
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        ws::log_error("epoll_ctl(ADD) failed for fd " + ws::itos(fd) + ": " + std::string(std::strerror(errno)));
        std::exit(EXIT_FAILURE);
    }
}

void EpollServer::removeFromEpoll(int fd)
{
    /* EPOLL_CTL_DEL ignores the event pointer on Linux >= 2.6.9 */
    epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
}

/* ───────────────────────── Event loop ──────────────────────────────── */

void EpollServer::run()
{
    ws::log_info("Entering event loop…");

    while (true)
    {
        int n = epoll_wait(_epfd, _events, MAX_EVENTS, -1);
        if (n == -1)
        {
            if (errno == EINTR)
                continue; /* Signal interrupted — not an error */
            ws::log_error("epoll_wait failed: " + std::string(std::strerror(errno)));
            break;
        }

        for (int i = 0; i < n; ++i)
        {
            int fd = _events[i].data.fd;

            if (fd == _listenFd)
            {
                acceptNewClient();
            }
            /* Sprint 2: handle EPOLLIN / EPOLLOUT for client fds here */
        }
    }
}

/* ───────────────────── Connection acceptance ───────────────────────── */

void EpollServer::acceptNewClient()
{
    /* Loop accept() — there may be multiple pending connections */
    while (true)
    {
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);

        int clientFd = accept(_listenFd,
                              reinterpret_cast<struct sockaddr *>(&clientAddr), &addrLen);

        if (clientFd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; /* All pending connections accepted */
            ws::log_error("accept() failed: " + std::string(std::strerror(errno)));
            break;
        }

        /* Make the new client fd non-blocking */
        setNonBlocking(clientFd);

        /* Register with epoll for read events */
        addToEpoll(clientFd, EPOLLIN);

        /* Store client state */
        _clients[clientFd] = ClientState(clientFd);

        ws::log_info("New client fd=" + ws::itos(clientFd) + " from " + std::string(inet_ntoa(clientAddr.sin_addr)) + ":" + ws::itos(ntohs(clientAddr.sin_port)));
    }
}
