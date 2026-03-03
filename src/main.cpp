#include "utils.hpp"
#include "EpollServer.hpp"

int main()
{
    signal(SIGPIPE, SIG_IGN);
    EpollServer server("0.0.0.0", 8080);
    server.init();
    server.run();
    return 0;
}
