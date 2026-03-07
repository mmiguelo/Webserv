#pragma once

#include <iostream>
#include <string>
#include <set>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include "ServerConfig.hpp"

#define MAX_EVENTS 64

class EpollServer
{
private:
    int _epollFd;
    std::set<int> _listenFds;
    std::map<int, ServerConfig*> _fdToConfig;

    struct epoll_event _events[MAX_EVENTS];

    int _createAndBindSocket(const std::string &host, int port);
    void _setNonBlocking(int fd);
    void _registerToEpoll(int fd, uint32_t events);
    void _acceptNewClient(int listenFd);
    void _handleClientData(int fd);

public:
    EpollServer();
    ~EpollServer();

    void addServer(ServerConfig &config);
    void run();
};
