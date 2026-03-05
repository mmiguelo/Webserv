#include "utils.hpp"

/* int main() {
    EpollServer server("0.0.0.0", 8080);
    server.init();
    server.run();
    return 0;
} */

#include <iostream>
#include <fstream>
#include <sstream>
#include "config/Tokenizer.hpp"
#include "config/Utils_config.hpp" // for debugPrintToken
#include "config/ConfigParser.hpp"
#include "config/configValidator.hpp"

void printServers(const std::vector<ServerConfig>& servers);

//test file

int main(int argc, char **argv)
{
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

        // 1️⃣ Tokenize
        Tokenizer tokenizer;
        std::vector<Token> tokens = tokenizer.tokenize(content);
        std::cout << "=== TOKENS ===\n";
        for (size_t i = 0; i < tokens.size(); i++)
            debugPrintToken(tokens[i]);

        // 2️⃣ Parse
        std::cout << "\n=== PARSING ===\n";
        ConfigParser parser(tokens);
        std::vector<ServerConfig> servers = parser.parse();

        // 3️⃣ Validate
        std::cout << "\n=== VALIDATING ===\n";
        ConfigValidator::validate(servers);
        std::cout << "\n✅ Config parsed and validated successfully\n";

        // 4️⃣ Print parsed config for verification
        printServers(servers);
    }
    catch (const std::exception& e) {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
    }
    return 0;
}

/* c++ -Wall -Wextra -Werror -std=c++98 \
-I includes \
src/config/test_main.cpp \
src/config/Tokenizer.cpp \
src/config/Utils_config.cpp \
src/config/ConfigValidator.cpp \
src/config/parser/ConfigParser.cpp \
src/config/parser/ConfigParserServer.cpp \
src/config/parser/ConfigParserServerDirectives.cpp \
src/config/parser/ConfigParserLocation.cpp \
src/config/parser/ConfigParserLocationDirectives.cpp \
-o test */

/* 
Tokenizer tokenizer;
std::vector<Token> tokens = tokenizer.tokenize(configContent);

ConfigParser parser(tokens);
std::vector<ServerConfig> servers = parser.parse();

ConfigValidator::validate(servers); */

void printServers(const std::vector<ServerConfig>& servers)
{
    for (size_t i = 0; i < servers.size(); i++)
    {
        const ServerConfig& server = servers[i];

        std::cout << "\n===== SERVER " << i << " =====\n";

        std::cout << "Host: " << server.host << "\n";
        std::cout << "Port: " << server.port << "\n";
        std::cout << "Root: " << server.root << "\n";

        std::cout << "Server names: ";
        for (size_t j = 0; j < server.server_name.size(); j++)
            std::cout << server.server_name[j] << " ";
        std::cout << "\n";

        std::cout << "Methods: ";
        for (size_t j = 0; j < server.methods.size(); j++)
            std::cout << server.methods[j] << " ";
        std::cout << "\n";

        std::cout << "Client max body size: " << server.client_max_body_size << "\n";

        std::cout << "Error pages:\n";
        for (std::map<int,std::string>::const_iterator it = server.error_page.begin();
             it != server.error_page.end();
             ++it)
        {
            std::cout << "  " << it->first << " -> " << it->second << "\n";
        }

        std::cout << "\nLocations:\n";

        for (size_t j = 0; j < server.locations.size(); j++)
        {
            const LocationConfig& loc = server.locations[j];

            std::cout << "  --- Location " << j << " ---\n";
            std::cout << "  Path: " << loc.path << "\n";
            std::cout << "  Root: " << loc.root << "\n";
            std::cout << "  Upload dir: " << loc.upload_dir << "\n";
            std::cout << "  Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";

            std::cout << "  Methods: ";
            for (size_t k = 0; k < loc.methods.size(); k++)
                std::cout << loc.methods[k] << " ";
            std::cout << "\n";

            std::cout << "  CGI: ";
            for (std::map<std::string,std::string>::const_iterator it = loc.cgi_ext.begin();
                 it != loc.cgi_ext.end();
                 ++it)
            {
                std::cout << "    " << it->first << " -> " << it->second << "\n";
            }

            if (loc.return_code != 0)
            {
                std::cout << "  Return: " << loc.return_code
                          << " -> " << loc.return_url << "\n";
            }
        }
    }
}