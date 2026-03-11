#pragma once

#define COLOR_BLUE "\033[34m"
#define COLOR_BLACK "\033[30m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_RESET "\033[0m"

#include <istream>
#include <iostream>
#include <string>
#include <map>
#include <cstdlib>
#include <cctype>
#include <sstream>
#include <vector>
#include <sstream>
#include <cerrno>

#include <csignal>
#include "HttpRequest.hpp"
#include "HttpParser.hpp"
#include "ServerConfig.hpp"

/*=============================================================================#
#                              UTILITY FUNCTIONS                               #
#=============================================================================*/

namespace utils
{
    void log_info(const std::string &message);
    void log_error(const std::string &message);
}

//http
std::string         toLowerStr(const std::string& str);
std::string         trimWhitespace(const std::string& str);
HttpMethod          stringToMethod(const std::string& method);
std::string         methodToString(HttpMethod method);
bool                isValidDecimal(const std::string& s);
bool                isValidHexadecimal(const std::string& s);
