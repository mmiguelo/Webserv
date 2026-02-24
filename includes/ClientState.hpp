/* ************************************************************************** */
/*                                                                            */
/*   ClientState.hpp                                                          */
/*                                                                            */
/*   Holds per-client data: socket fd, read buffer, write buffer,             */
/*   and (later) the parsed request and response objects.                     */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENTSTATE_HPP
#define CLIENTSTATE_HPP

#include <string>

struct ClientState
{
    int fd;
    std::string readBuf;
    std::string writeBuf;

    ClientState() : fd(-1) {}
    explicit ClientState(int clientFd) : fd(clientFd) {}
};

#endif
