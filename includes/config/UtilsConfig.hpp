#ifndef UTILS_CONFIG_HPP
#define UTILS_CONFIG_HPP

#include "Token.hpp"
#include <string>

std::string tokenTypeToString(TokenType type);
void debugPrintToken(const Token& token);
bool isNumber(const std::string& str);
std::string normalizePath(const std::string& path);
std::string toAbsolutePath(const std::string& path);

#endif