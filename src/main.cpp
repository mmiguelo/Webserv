/* ************************************************************************** */
/*                                                                            */
/*   main.cpp                                                                 */
/*                                                                            */
/*   Entry point — creates the EpollServer and starts the event loop.         */
/*                                                                            */
/* ************************************************************************** */

#include "EpollServer.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <csignal>

int main(int argc, char **argv)
{
    (void)argv;
    if (argc > 2)
    {
        ws::log_error("Usage: ./webserv [config_file]");
        return EXIT_FAILURE;
    }

    /* Ignore SIGPIPE — writing to a closed socket should return an error,
       not kill the entire server. */
    std::signal(SIGPIPE, SIG_IGN);

    /* For Sprint 1 the host and port are hard-coded.
       Sprint 3 will read them from the config file. */
    std::string host = "127.0.0.1";
    int port = 8080;

    ws::log_info("webserv starting on " + host + ":" + ws::itos(port));

    EpollServer server(host, port);
    server.run();

    return EXIT_SUCCESS;
}