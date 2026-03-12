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
    if (path.length() > 1 && path[path.length() - 1] == '/')
        return path.substr(0, path.length() - 1);
    return path;
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
	// Already absolute
	if (path[0] == '/')
		return path;
	// Convert relative to absolute
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return path; // fallback to original if getcwd fails
	return (std::string(cwd) + "/" + path);
}