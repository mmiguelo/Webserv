#pragma once

#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpRouter.hpp"
#include "ServerConfig.hpp"
#include <string>
#include <ctime>
#include <sys/epoll.h>
#include <sys/stat.h>

#define MAX_TIMEOUT 15

class EpollClient
{
private:
    int _fd;
    int _epollFd;
    ServerConfig *_config;

    HttpParser _parser;
    std::string _sendBuffer;
    time_t _lastActivity;
    bool _continueSent;
    bool _closeAfterSend;

    void _processPipelines();
    void _createResponse(bool complete);

    bool _keepAlive(const HttpRequest &request) const;
    bool _buildErrorResponse(const HttpRequest &request, HttpResponse &response, std::string &responseStr);
    void _buildRoutedResponse(const HttpRequest &request, HttpResponse &response, std::string &responseStr);
    void _buildFromFile(const HttpRequest &request, HttpResponse &response, const HttpRouteMatch &match, std::string &responseStr);
    void _finalizeResponse(const HttpRequest &request, HttpResponse &response, std::string &responseStr, bool keepAlive);
    void _switchToWrite();
    void _switchToRead();

public:
    EpollClient(int fd, int epollFd, ServerConfig *config);

    bool handleRead();
    bool handleWrite();

    bool isTimedOut(time_t now) const;
    int getFd() const;
};
