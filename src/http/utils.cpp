#include "utils.hpp"

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