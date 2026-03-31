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
	// Paths like "/www/html" are treated as relative to cwd, not system root
	char *cwd;
    cwd = std::getenv("PWD");
	if (cwd == NULL)
		return path; // fallback to original if getcwd fails
	std::string cwdStr(cwd);
	// Ensure there's a slash between cwd and path
	if (!cwdStr.empty() && cwdStr[cwdStr.length() - 1] != '/' && path[0] != '/')
		cwdStr += "/";
	return (cwdStr + path);
}

std::string getErrorMessage(int statusCode) {
    static std::map<int, std::string> errorMessage;
    
    if (errorMessage.empty()) {
        errorMessage[403] = "It looks like you're not allowed to be here. If you think this is a mistake, it might be worth double-checking your permissions.";
        errorMessage[404] = "We're fairly sure that page used to be here, but it seems to have gone missing. We do apologise on its behalf.";
        errorMessage[405] = "Well... that method isn't going to work here. Maybe try a different approach?";
        errorMessage[408] = "That took a bit too long. The server gave up waiting—maybe give it another go?";
        errorMessage[413] = "That's quite a large request! The server couldn't handle it this time.";
        errorMessage[414] = "That URL is impressively long... a bit *too* long for us to handle, unfortunately.";
        errorMessage[431] = "Some of the request details are a bit too large to process. Trimming them down should help.";
        errorMessage[500] = "Something went wrong on our side. We're probably already looking into it (or at least we should be). Sorry about that!";
        errorMessage[501] = "That feature isn't supported yet. It might be on the to-do list... somewhere.";
        errorMessage[503] = "We're a bit overwhelmed right now. Please try again in a little while.";
        errorMessage[505] = "That HTTP version isn't something we can work with. A different one should do the trick.";
    };

    std::map<int, std::string>::const_iterator it = errorMessage.find(statusCode);
    if (it != errorMessage.end())
        return it->second;

    return "Something unexpected happened. We're not quite sure what, but it might be worth trying again.";
}
