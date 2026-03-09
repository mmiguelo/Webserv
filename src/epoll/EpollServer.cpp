#include "EpollServer.hpp"
#include "HttpParser.hpp"

EpollServer::EpollServer() : _epollFd(-1)
{
    _epollFd = epoll_create1(0);
    if (_epollFd == -1)
        throw std::runtime_error("epoll_create1 failed");
}

EpollServer::~EpollServer() {
    for (std::set<int>::iterator it = _listenFds.begin(); it != _listenFds.end(); ++it)
        close(*it);
    if (_epollFd != -1)
        close(_epollFd);
}

void EpollServer::_setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // get the binary flag for fd
    if (flags == -1)
        throw std::runtime_error("fcntl getfl failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) // set the flag to nonblocking
        throw std::runtime_error("fcntl setfl failed");
}

void EpollServer::_registerToEpoll(int fd, uint32_t events)
{
    struct epoll_event epoll_ev;
    epoll_ev.events = events;
    epoll_ev.data.fd = fd;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &epoll_ev) == -1) // add the new fd to the list
        throw std::runtime_error("epoll ctl failed");
}

int EpollServer::_createAndBindSocket(const std::string &host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(fd);
        throw std::runtime_error("setsockopt failed");
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host.c_str());

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        close(fd);
        throw std::runtime_error("bind failed");
    }
    if (listen(fd, SOMAXCONN) == -1)
    {
        close(fd);
        throw std::runtime_error("listen failed");
    }
    return fd;
}

void EpollServer::addServer(ServerConfig &config)
{
    int fd = _createAndBindSocket(config.getHost(), config.getPort());
    _setNonBlocking(fd);
    _registerToEpoll(fd, EPOLLIN);
    _listenFds.insert(fd);
    _fdToConfig[fd] = &config;

    std::ostringstream oss;
    oss << config.getPort();
    utils::log_info("Listening on " + config.getHost() + ":" + oss.str());
}


void EpollServer::_acceptNewClient(int listenFd)
{
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listenFd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            utils::log_error("accept() failed");
            break;
        }
        _setNonBlocking(client_fd);
        _registerToEpoll(client_fd, EPOLLIN);
        // TODO: map client_fd → the ServerConfig* from _fdToConfig[listenFd]
    }
}

void EpollServer::_handleClientData(int fd)
{
    HttpParser parser;
    char buffer[10000];

    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead <= 0)
    {
        return;
    }
    buffer[bytesRead] = '\0';

    bool complete = parser.feed(buffer);
    if (complete)
    {
        std::cout << "\n=== Request Complete ===" << std::endl;
        parser.getRequest().print(std::cout);
    }
    else if (parser.getState() == PARSE_ERROR)
    {
        std::cout << "\n=== Parse Error ===" << std::endl;
        parser.getRequest().print(std::cout);
    }

    // Make this more dynamic with custom variables on task 2
    std::ostringstream oss;
    HttpRequest request = parser.getRequest();
    oss << request.getVersion() << " " << request.getErrorCode() << " OK\r\n"
        << "Content-Type: text/plain\r\n"
        << "Content-Length: " << "11" << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << "Hello World";
    std::string response = oss.str();
    write(fd, response.c_str(), response.size());
}

void EpollServer::run()
{
    while (true)
    {
        int n = epoll_wait(_epollFd, _events, MAX_EVENTS, -1);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("epoll_wait failed");
        }
        for (int i = 0; i < n; i++)
        {
            int fd = _events[i].data.fd;

            if (_listenFds.count(fd))       // it's a listen socket → accept
                _acceptNewClient(fd);
            else                            // it's a client socket → handle
            {
                _handleClientData(fd);
                close(fd);
            }
        }
    }
}
