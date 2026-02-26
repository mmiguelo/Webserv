#include "EpollServer.hpp"
#include "HttpParser.hpp"

EpollServer::EpollServer(const std::string &host, int port) : _listenFd(-1), _epollFd(-1), _port(port), _host(host) {}
EpollServer::~EpollServer() {}

void EpollServer::_createSocket()
{
    _listenFd = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP listen socket. 0 is the default protocol.
    if (_listenFd == -1)
        throw std::runtime_error("Error creating listen fd socket.");

    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) // set socket options. In this case, SOL_SOCKET is the generic level and SO_REUSEADDR tells the kernel to reuse the address even if it is waiting
        throw std::runtime_error("Error ");
}

void EpollServer::_setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // get the binary flag for fd
    if (flags == -1)
        throw std::runtime_error("fcntl getfl failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) // set the flag to nonblocking
        throw std::runtime_error("fcntl setfl failed");
}

void EpollServer::_bindAndListen()
{
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;                       // set the family to the same family of socket
    addr.sin_port = htons(_port);                    // network byte order is big-endian, while many machines have little-endian. We need to transform the port to big-endian style
    addr.sin_addr.s_addr = inet_addr(_host.c_str()); // convert address into binary number in network big-endian order

    if (bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) == -1) // associates socket to host + port
        throw std::runtime_error("bind failed");

    if (listen(_listenFd, SOMAXCONN) == -1) // start listening connections
        throw std::runtime_error("listen failed");
}

void EpollServer::_registerToEpoll(int fd, uint32_t events)
{
    struct epoll_event epoll_ev;
    epoll_ev.events = events;
    epoll_ev.data.fd = fd;

    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &epoll_ev) == -1) // add the new fd to the list
        throw std::runtime_error("epoll ctl failed");
}

void EpollServer::_acceptNewClient()
{
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // Creates new socket for this client, fills client_addr with client IP and port and returns the new fd
        int client_fd = accept(_listenFd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            utils::log_error("accept() failed");
            break;
        }

        _setNonBlocking(client_fd);
        _registerToEpoll(client_fd, EPOLLIN);
        std::cout << "New Client fd = " << client_fd << std::endl;
        //close(client_fd);
        //TODO: probably it needs to be passed information to HttpParser here
    }
}

void EpollServer::init()
{
    _epollFd = epoll_create1(0);
    _createSocket();
    _setNonBlocking(_listenFd);
    _bindAndListen();
    _registerToEpoll(_listenFd, EPOLLIN);

    utils::log_info("Server listening");
}

void EpollServer::_handleClientData(int fd) {
    HttpParser parser;
    char buffer[10000];

    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
    if (bytesRead <= 0) {
        return;
    }
    buffer[bytesRead] = '\0';

    bool complete = parser.feed(buffer);
    if (complete) {
        std::cout << "\n=== Request Complete ===" << std::endl;
        parser.getRequest().print(std::cout);
    } else if (parser.getState() == PARSE_ERROR) {
        std::cout << "\n=== Parse Error ===" << std::endl;
        parser.getRequest().print(std::cout);
    }

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
            throw std::runtime_error("epoll did not waited");
        }

        for (int i = 0; i < n; i++)
        {
            int fd = _events[i].data.fd;

            if (fd == _listenFd)
                _acceptNewClient();
            else {
                _handleClientData(fd);
                close(fd);
            }
        }
    }
}
