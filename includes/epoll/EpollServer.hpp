#pragma once

#include <iostream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <set>
#include <map>
#include "ServerConfig.hpp"
#include "HttpParser.hpp"

#define MAX_EVENTS 64
#define MAX_TIMEOUT 10

struct ClientData
{
    std::string recv_buf;
    std::string send_buf;
    int server_fd;
    ServerConfig *server_config;
    time_t last_activity;
    HttpParser parser;
    bool continue_sent;
};

class EpollServer
{
private:
    int _epollFd;
    std::set<int> _listenFds;
    std::map<int, ServerConfig *> _fdToConfig;
    std::map<int, ClientData> _clients;

    struct epoll_event _events[MAX_EVENTS];

    int _createAndBindSocket(const std::string &host, int port);
    void _setNonBlocking(int fd);
    void _registerToEpoll(int fd, uint32_t events);
    void _acceptNewClient(int listenFd);
    void _handleClientData(int fd);
    void _closeClient(int fd);
    void _handleClientResponse(int fd);
    void _checkTimeout();
    void _createResponse(int fd, bool complete, ClientData &data);

public:
    EpollServer();
    ~EpollServer();

    void addServer(ServerConfig &config, int port);
    void run();
};
