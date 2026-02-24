/* ************************************************************************** */
/*                                                                            */
/*   EpollServer.hpp                                                          */
/*                                                                            */
/*   Core event-loop class. Creates the listening socket, registers it        */
/*   with epoll, and runs the main event loop.                                */
/*                                                                            */
/* ************************************************************************** */

#ifndef EPOLLSERVER_HPP
#define EPOLLSERVER_HPP

#include <string>
#include <map>
#include <sys/epoll.h>
#include "ClientState.hpp"

class EpollServer
{
public:
    EpollServer(const std::string &host, int port);
    ~EpollServer();

    void run(); /* Main event loop */

private:
    /* Disable copy — a server should never be copied */
    EpollServer(const EpollServer &);
    EpollServer &operator=(const EpollServer &);

    /* Setup helpers */
    int createListenSocket();
    void setNonBlocking(int fd);
    void addToEpoll(int fd, uint32_t events);
    void removeFromEpoll(int fd);

    /* Event handlers */
    void acceptNewClient();

    /* Data members */
    std::string _host;
    int _port;
    int _listenFd;
    int _epfd;

    static const int MAX_EVENTS = 64;
    struct epoll_event _events[MAX_EVENTS];

    std::map<int, ClientState> _clients;
};

#endif
