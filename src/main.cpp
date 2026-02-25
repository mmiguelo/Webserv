#include "utils.hpp"
#include "EpollServer.hpp"

int main()
{
    EpollServer server("0.0.0.0", 8080);
    server.init();
    server.run();
    return 0;
}