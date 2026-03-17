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
#define MAX_TIMEOUT 30

struct ClientData
{
    std::string recv_buf;
    std::string send_buf;
    int server_fd;
    ServerConfig *server_config;
    time_t last_activity;
    HttpParser parser;
    bool continue_sent;
    bool should_close_after_send;
    ClientData() : server_fd(-1), server_config(NULL), last_activity(0), continue_sent(false), should_close_after_send(false) {}
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
    void _createResponse(int fd, bool complete, ClientData *data);
    void _processPipelines(int fd, ClientData *data);
    void _verifyGetAddr(int ret, int fd);
    void _verifyBind(int fd, struct addrinfo *res, std::ostringstream *oss, const std::string &host);
    void _verifyListen(int fd);
    bool _keepAlive(const HttpRequest &request);
    std::string EpollServer::_buildResponse(ClientData *data, const HttpRequest &request, int statusCode);
    std::string _selectResponse(int, bool, ClientData*, const HttpRequest&);
    void _queueResponse(int, ClientData*, const std::string&);

public:
    EpollServer();
    ~EpollServer();

    void addServer(ServerConfig *config, int port);
    void run();
};
