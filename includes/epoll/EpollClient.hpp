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
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <signal.h>
class EpollServer;
class HttpResponse;

#define MAX_TIMEOUT 1

class EpollClient
{
private:
    int             _fd;
    int             _epollFd;
    ServerConfig    *_config;
    EpollServer     *_server;

    HttpParser      _parser;
    std::string     _sendBuffer;
    time_t          _lastActivity;
    bool            _continueSent;
    bool            _closeAfterSend;

    // ==== CGI ====
    pid_t           _cgi_pid;
    int             _cgi_stdin_fd;
    int             _cgi_stdout_fd;
    std::string     _cgi_input_buffer;
    std::string     _cgi_output_buffer;
    size_t          _cgi_input_offset;
    time_t          _cgi_start_time;
    bool            _cgi_finished;

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
    EpollClient(int fd, int epollFd, ServerConfig *config, EpollServer *server);

    bool handleRead();
    bool handleWrite();

    bool isTimedOut(time_t now) const;
    int getFd() const;

    // === CGI getters ====
    pid_t getCgiPid() const;
    int getCgiStdinFd() const;
    int getCgiStdoutFd() const;
    std::string getCgiInputBuffer() const;
    std::string getCgiOutputBuffer() const;
    size_t getCgiInputOffset() const;
    time_t getCgiStartTime() const;

    void setSendBuffer(const std::string &newBuffer);
    std::string getSendBuffer() const;

    void startCgi(pid_t pid, int stdinFd, int stdoutFd, const std::string &body);

    // === CGI modifiers ====
    void setCgiInputOffset(size_t value);
    void setCgiStdinFd(int fd);
    void setCgiStdoutFd(int fd);
    void setCgiPid(int pid);
    void appendCgiStdoutBuffer(const std::string &other, size_t size);
    void setCgiDone(bool flag);
};
