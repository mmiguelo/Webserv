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
#include <set>
#include <map>
#include "HttpParser.hpp"

#define MAX_EVENTS 64
#define MAX_TIMEOUT 10

struct ClientData
{
    std::string recv_buf;
    std::string send_buf;
    int server_fd;
    time_t last_activity;
    HttpParser parser;
};

class EpollServer
{
private:
    int _listenFd;
    int _epollFd;
    int _port;
    std::string _host;
    std::map<int, ClientData> _clients;
    std::set<int> _listenSet;

    struct epoll_event _events[MAX_EVENTS];

    void _createSocket();
    void _setNonBlocking(int fd);
    void _bindAndListen();
    void _registerToEpoll(int fd, uint32_t events);
    void _acceptNewClient();
    void _handleClientData(int fd);
    void _closeClient(int fd);
    void _handleClientResponse(int fd);
    void _checkTimeout();

public:
    EpollServer(const std::string &host, int port);
    ~EpollServer();

    void init();
    void run();
};
