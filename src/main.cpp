#include "utils.hpp"
#include "EpollServer.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include "Tokenizer.hpp"
#include "UtilsConfig.hpp" // for debugPrintToken
#include "ConfigParser.hpp"
#include "Validator.hpp"

void printServers(const std::map<int, std::vector<ServerConfig> > &servers);

// int main(int argc, char **argv)
// {
//     signal(SIGPIPE, SIG_IGN);
//     std::map<int, std::vector<ServerConfig> > servers;
//     try
//     {
//         std::ifstream file;
//         if (argc != 2)
//         {
//             file.open("config/test.conf");
//             if (!file.is_open())
//             {
//                 std::cerr << "Failed to open file\n";
//                 return 1;
//             }
//         }
//         else
//         {
//             file.open(argv[1]);
//             if (!file.is_open())
//             {
//                 std::cerr << "Failed to open file\n";
//                 return 1;
//             }
//         }
//         std::stringstream buffer;
//         buffer << file.rdbuf();
//         std::string content = buffer.str();

//         std::vector<Token> tokens = Tokenizer::tokenize(content);
//         /* for (size_t i = 0; i < tokens.size(); i++)
//             debugPrintToken(tokens[i]); */
//         ConfigParser parser(tokens);
//         parser.parse(servers);
//         Validator::validate(servers);
//         std::cout << "\nConfig parsed and validated successfully\n";

//         // 4️⃣ Print parsed config
//         //printServers(servers);

//         EpollServer server;
//         std::map<int, ServerConfig>::iterator it;
//         std::cout << servers.size() << " server(s) to add to EpollServer\n";
//         for (it = servers.begin(); it != servers.end(); ++it)
//         {
//             std::vector<int> ports = it->second.getPorts();
//             for (std::vector<int>::const_iterator portIt = ports.begin(); portIt != ports.end(); ++portIt)
//             {
//                 std::cout << *portIt << std::endl;
//                 server.addServer(it->second, *portIt);
//             }
//         }
//         server.run();
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "❌ ERROR: " << e.what() << std::endl;
//         return 1;
//     }
// }

int main(int argc, char **argv) //MAIN ISOLADA PARA TESTAR AS MUDANCAS NO PARSER
{
    signal(SIGPIPE, SIG_IGN);
    try
    {
        std::ifstream file;

        if (argc != 2)
            file.open("config/test.conf");
        else
            file.open(argv[1]);

        if (!file.is_open())
        {
            std::cerr << "Failed to open file\n";
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // 1️⃣ Tokenize
        std::vector<Token> tokens = Tokenizer::tokenize(content);
        ConfigParser parser(tokens);
        std::map<int, std::vector<ServerConfig> > servers;
        parser.parse(servers);
        Validator::validate(servers);
        std::cout << "\n✅ Config parsed and validated successfully\n";
        printServers(servers);
    }
    catch (const std::exception &e)
    {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void printServers(const std::map<int, std::vector<ServerConfig> > &servers)
{
    for (std::map<int, std::vector<ServerConfig> >::const_iterator serverIt = servers.begin();
         serverIt != servers.end();
         ++serverIt) {
        std::cout << "\n---- PORT " << serverIt->first << " ----\n";
        const std::vector<ServerConfig> &serverList = serverIt->second;

        for (size_t s = 0; s < serverList.size(); s++) {
            const ServerConfig &server = serverList[s];
            std::cout << "\nserver {\n";
            // LISTEN DIRECTIVES
            const std::vector<ServerConfig::ListenDirective> &listens =
                server.getListenDirectives();
            for (size_t i = 0; i < listens.size(); i++) {
                std::cout << "    listen "
                          << listens[i].host << ":"
                          << listens[i].port << ";\n";
            }

            // ROOT
            std::cout << "    root " << server.getRoot() << ";\n";

            // SERVER NAMES
            const std::vector<std::string> &names = server.getServerName();
            if (!names.empty()) {
                std::cout << "    server_name ";
                for (size_t i = 0; i < names.size(); i++)
                    std::cout << names[i] << " ";
                std::cout << ";\n";
            }

            // METHODS
            const std::vector<std::string> &methods = server.getMethods();
            if (!methods.empty()) {
                std::cout << "    methods ";
                for (size_t i = 0; i < methods.size(); i++)
                    std::cout << methods[i] << " ";
                std::cout << ";\n";
            }

            // CLIENT BODY SIZE
            std::cout << "    client_max_body_size "
                      << server.getClientMaxBodySize() << ";\n";

            // ERROR PAGES
            const std::map<int, std::string> &errorPages = server.getErrorPage();
            for (std::map<int, std::string>::const_iterator errorIt = errorPages.begin();
                 errorIt != errorPages.end();
                 ++errorIt) {
                std::cout << "    error_page "
                          << errorIt->first << " "
                          << errorIt->second << ";\n";
            }

            // LOCATIONS
            const std::vector<LocationConfig> &locations = server.getLocations();
            for (size_t i = 0; i < locations.size(); i++) {
                const LocationConfig &loc = locations[i];
                std::cout << "\n    location " << loc.path << " {\n";
                if (!loc.root.empty())
                    std::cout << "        root " << loc.root << ";\n";
                if (!loc.upload_dir.empty())
                    std::cout << "        upload_dir " << loc.upload_dir << ";\n";
                std::cout << "        autoindex "
                          << (loc.autoindex ? "on" : "off") << ";\n";
                if (!loc.methods.empty()) {
                    std::cout << "        methods ";
                    for (size_t j = 0; j < loc.methods.size(); j++)
                        std::cout << loc.methods[j] << " ";
                    std::cout << ";\n";
                }
                for (std::map<std::string, std::string>::const_iterator cgiIt = loc.cgi_ext.begin();
                     cgiIt != loc.cgi_ext.end();
                     ++cgiIt) {
                    std::cout << "        cgi "
                              << cgiIt->first << " "
                              << cgiIt->second << ";\n";
                }
                if (loc.redirect_code != 0) {
                    std::cout << "        return "
                              << loc.redirect_code << " "
                              << loc.redirect_url << ";\n";
                }
                std::cout << "    }\n";
            }
            std::cout << "}\n";
        }
    }
}