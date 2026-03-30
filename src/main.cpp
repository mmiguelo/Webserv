#include "utils.hpp"
#include "EpollServer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include "Tokenizer.hpp"
#include "utils.hpp" // for debugPrintToken
#include "ConfigParser.hpp"
#include "Validator.hpp"

void printServers(const std::map<int, std::vector<ServerConfig> > &servers);

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    std::map<int, std::vector<ServerConfig> > servers;
    try
    {
        std::ifstream file;
        if (argc != 2)
        {
            file.open("config/test.conf");
            if (!file.is_open())
            {
                std::cerr << "Failed to open file\n";
                return 1;
            }
        }
        else
        {
            file.open(argv[1]);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file\n";
                return 1;
            }
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        std::vector<Token> tokens = Tokenizer::tokenize(content);
        ConfigParser parser(tokens);
        parser.parse(servers);
        Validator::validate(servers);

        EpollServer server;
        std::map<int, std::vector<ServerConfig> >::iterator it;
        for (it = servers.begin(); it != servers.end(); ++it)
        {
            int port = it->first;
            std::vector<ServerConfig>& serverList = it->second;
            for (std::vector<ServerConfig>::iterator serverIt = serverList.begin(); serverIt != serverList.end(); ++serverIt) {
                server.addServer(&(*serverIt), port);
            }
        }
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}
