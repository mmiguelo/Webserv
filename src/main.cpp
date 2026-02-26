#include "../includes/http/utils.hpp"
#include "../includes/http/HttpParser.hpp"

int main() {
    HttpParser parser;

    std::string buffer = "GET /path/resource?query=val HTTP/1.1\r\n"
                         "Host: localhost:8080\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 12\r\n"
                         "\r\n"
                         "hello world!";

    bool complete = parser.feed(buffer);
    if (complete) {
        std::cout << "\n=== Request Complete ===" << std::endl;
        parser.getRequest().print(std::cout);
    } else if (parser.getState() == PARSE_ERROR) {
        std::cout << "\n=== Parse Error ===" << std::endl;
        parser.getRequest().print(std::cout);
    }

    return 0;
}
