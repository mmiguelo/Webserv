#pragma once

#include "EpollClient.hpp"
#include "ServerConfig.hpp"
#include <map>
#include <set>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <cstring>
#include <sys/wait.h>

#define MAX_EVENTS 64
#define CGI_TIMEOUT 30

class EpollServer
{
private:
    int                             _epollFd;
    std::set<int>                   _listenFds;
    std::map<int, EpollClient *>    _clients;
    std::map<int, ServerConfig *>   _fdToConfig;
    struct epoll_event              _events[MAX_EVENTS];

    // ==== CGI ====
    std::map<int, int>              _cgi_fds;

    void _setNonBlocking(int fd);
    void _registerToEpoll(int fd, uint32_t events);
    int _createAndBindSocket(const std::string &host, int port);
    void _acceptNewClient(int listenFd);
    void _closeClient(int fd);
    void _checkTimeout();

    void _verifyGetAddr(int ret, int fd);
    void _verifyBind(int fd, struct addrinfo *res, std::ostringstream *oss, const std::string &host);
    void _verifyListen(int fd);

    // ==== CGI ====
    void _closeCgiFd(int fd);
    void _handleCgiWrite(int stdinFd, EpollClient *client);
    void _handleCgiRead(int stdoutFd, EpollClient* client);
    void _checkCgiTimeouts();

public:
    EpollServer();
    ~EpollServer();

    void addServer(ServerConfig *config, int port);
    void run();

    // ==== CGI ====
    void registerCgi(int clientFd, int cgiStdinFd, int cgiStdoutFd);
};
