#include "utils.hpp"
#include "config/Tokenizer.hpp"
#include <iostream>
#include <unistd.h>
#include <climits>

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
    switch (method)
    {
        case METHOD_GET:    return "GET";
        case METHOD_POST:   return "POST";
        case METHOD_DELETE: return "DELETE";
        case METHOD_HEAD:   return "HEAD";
        default:            return "UNKNOWN";
    }
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

//####------DEBUG------####
std::string tokenTypeToString(TokenType type) {
	switch (type) {
		case WORD: return "WORD";
		case LBRACE: return "LBRACE";
		case RBRACE: return "RBRACE";
		case SEMICOLON: return "SEMICOLON";
		default: return "UNKNOWN";
	}
}

void debugPrintToken(const Token& token) {
std::cout << "Token type-> "
		  << tokenTypeToString(token.type)
		  << "; value-> \"" << token.value
		  << "\"; line-> " << token.lineNum
		  << "]"
		  << '\n';
}

std::string normalizePath(const std::string& path)
{
	std::string finalPath = path;
    while (finalPath.length() > 1 && finalPath[finalPath.length() - 1] == '/')
        finalPath.erase(finalPath.length() - 1);
    return finalPath;
}

bool isNumber(const std::string& str) {
	if (str.empty())
		return false;
	for (size_t i = 0; i < str.size(); i++) {
		if (!std::isdigit(str[i]))
			return false;
	}
	return true;
}

std::string toAbsolutePath(const std::string& path) {
	if (path.empty())
		return path;
	// Convert relative to absolute (prepend cwd)
	char* cwd = std::getenv("PWD");
	if (!cwd)
		return path; // fallback to original if getcwd fails
	std::string cwdStr(cwd);
    std::string fullPath;
    if (path[0] == '/')
		fullPath = path;
	else {
		// Ensure there's a slash between cwd and path
		if (!cwdStr.empty() && cwdStr[cwdStr.length() - 1] != '/')
			cwdStr += "/";
		fullPath = cwdStr + path;
	} //an example would be: 
	return normalizePath(fullPath);
}

std::string normalizePath(const std::string& path)
{
	std::vector<std::string> parts;
	std::stringstream ss(path); // to split the path between '/'
	std::string item;

	while (std::getline(ss, item, '/')) { // read till '/'
		if (item.empty() || item == ".")
			continue; // ignore useless parts
		if (item == "..") {
			if (!parts.empty())
				parts.pop_back(); // we want to go one folder back
                //so, rm the last part; example:
                // "bom/dia/vietname" becomes "bom/dia"
		} else
			parts.push_back(item); // add this bit
	}
	std::string result;
	for (size_t i = 0; i < parts.size(); i++)
		result += "/" + parts[i];

	if (result.empty())
		result = "/";
	return result;
}