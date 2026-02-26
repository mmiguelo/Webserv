#pragma once

#include <iostream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>
#include <unistd.h>

#define MAX_EVENTS 64

class EpollServer
{
private:
    int _listenFd;
    int _epollFd;
    int _port;
    std::string _host;

    struct epoll_event _events[MAX_EVENTS];

    void _createSocket();
    void _setNonBlocking(int fd);
    void _bindAndListen();
    void _registerToEpoll(int fd, uint32_t events);
    void _acceptNewClient();
    void _handleClientData(int fd);

public:
    EpollServer(const std::string &host, int port);
    ~EpollServer();

    void init();
    void run();
};
