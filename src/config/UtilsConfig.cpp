#include "config/Token.hpp"
#include "config/UtilsConfig.hpp"
#include <iostream>
#include <unistd.h>
#include <climits>


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
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return path; // fallback to original if getcwd fails
	std::string cwdStr(cwd);
	// Ensure there's a slash between cwd and path
	if (!cwdStr.empty() && cwdStr[cwdStr.length() - 1] != '/' && path[0] != '/')
		cwdStr += "/";
	return (cwdStr + path);
}