#include "utils.hpp"

namespace utils
{
    void log_error(const std::string &message)
    {
        std::cerr << COLOR_RED << "[ERROR]: " << COLOR_RESET << message << std::endl;
    }

    void log_info(const std::string &message)
    {
        std::cout << COLOR_YELLOW << "[INFO]: " << COLOR_RESET << message << std::endl;
    }
}

std::string toLowerStr(const std::string& str) {
    std::string result = str;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = std::tolower(result[i]);
    return result;
}

std::string trimWhitespace(const std::string& str) {
    std::string::size_type start = str.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";
    std::string::size_type end = str.find_last_not_of(" \t\r");
    return str.substr(start, end - start + 1);
}

HttpMethod stringToMethod(const std::string& method) {
    if (method == "GET")
        return METHOD_GET;
    if (method == "POST")
        return METHOD_POST;
    if (method == "DELETE")
        return METHOD_DELETE;
    if (method == "HEAD")
        return METHOD_HEAD;
    return METHOD_UNKNOWN;
}

std::string methodToString(HttpMethod method) {
    if (method == METHOD_GET)
        return "GET";
    if (method == METHOD_POST)
        return "POST";
    if (method == METHOD_DELETE)
        return "DELETE";
    if (method == METHOD_HEAD)
        return "HEAD";
    return "UNKNOWN";
}

bool isValidDecimal(const std::string& s)
{
    if (s.empty())
        return false;

    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(s[i]))
            return false;

    // Now checks overflow
    errno = 0;
    std::strtoul(s.c_str(), NULL, 10);
    if (errno == ERANGE)
        return false;
    return true;
}

bool isValidHexadecimal(const std::string& s)
{
    if (s.empty())
        return false;

    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isxdigit(s[i]))
            return false;

    // Check overflow
    errno = 0;
    std::strtoul(s.c_str(), NULL, 16);
    if (errno == ERANGE)
        return false;
    return true;
}