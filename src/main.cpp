#include "utils.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include "Tokenizer.hpp"
#include "UtilsConfig.hpp" // for debugPrintToken
#include "ConfigParser.hpp"
#include "Validator.hpp"

void printServers(const std::map<int, ServerConfig> &servers);

int main(int argc, char **argv)
{
    std::map<int, ServerConfig> servers;
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
        //for (size_t i = 0; i < tokens.size(); i++)
        //    debugPrintToken(tokens[i]);
        ConfigParser parser(tokens);
        servers = parser.parse();
        Validator::validate(servers);
        std::cout << "\nConfig parsed and validated successfully\n";

        // 4️⃣ Print parsed config
        printServers(servers);

        EpollServer server;
        std::map<int, ServerConfig>::iterator it;
        for (it = servers.begin(); it != servers.end(); ++it)
            server.addServer(it->second);
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    }
}

void printServers(const std::map<int, ServerConfig> &servers)
{
    for (std::map<int, ServerConfig>::const_iterator serverIt = servers.begin();
         serverIt != servers.end();
         ++serverIt)
    {
        const ServerConfig &server = serverIt->second;

        std::cout << "\nserver {\n";

        std::cout << "    listen " << server.getPort() << ";\n";
        std::cout << "    root " << server.getRoot() << ";\n";

        const std::vector<std::string> &names = server.getServerName();
        if (!names.empty())
        {
            std::cout << "    server_name ";
            for (size_t i = 0; i < names.size(); i++)
                std::cout << names[i] << " ";
            std::cout << ";\n";
        }

        const std::vector<std::string> &methods = server.getMethods();
        if (!methods.empty())
        {
            std::cout << "    methods ";
            for (size_t i = 0; i < methods.size(); i++)
                std::cout << methods[i] << " ";
            std::cout << ";\n";
        }

        std::cout << "    client_max_body_size "
                  << server.getClientMaxBodySize() << ";\n";

        const std::map<int, std::string> &errorPages = server.getErrorPage();
        for (std::map<int, std::string>::const_iterator errorIt = errorPages.begin();
             errorIt != errorPages.end();
             ++errorIt)
        {
            std::cout << "    error_page "
                      << errorIt->first << " "
                      << errorIt->second << ";\n";
        }

        const std::vector<LocationConfig> &locations = server.getLocations();

        for (size_t i = 0; i < locations.size(); i++)
        {
            const LocationConfig &loc = locations[i];

            std::cout << "\n    location " << loc.path << " {\n";

            if (!loc.root.empty())
                std::cout << "        root " << loc.root << ";\n";

            if (!loc.upload_dir.empty())
                std::cout << "        upload_dir " << loc.upload_dir << ";\n";

            std::cout << "        autoindex "
                      << (loc.autoindex ? "on" : "off") << ";\n";

            if (!loc.methods.empty())
            {
                std::cout << "        methods ";
                for (size_t j = 0; j < loc.methods.size(); j++)
                    std::cout << loc.methods[j] << " ";
                std::cout << ";\n";
            }

            for (std::map<std::string, std::string>::const_iterator cgiIt = loc.cgi_ext.begin();
                 cgiIt != loc.cgi_ext.end();
                 ++cgiIt)
            {
                std::cout << "        cgi "
                          << cgiIt->first << " "
                          << cgiIt->second << ";\n";
            }

            if (loc.redirect_code != 0)
            {
                std::cout << "        return "
                          << loc.redirect_code << " "
                          << loc.redirect_url << ";\n";
            }

            std::cout << "    }\n";
        }

        std::cout << "}\n";
    }
}