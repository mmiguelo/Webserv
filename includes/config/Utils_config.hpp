#ifndef UTILS_CONFIG_HPP
#define UTILS_CONFIG_HPP

#include "Token.hpp"
#include <string>

std::string tokenTypeToString(TokenType type);
void debugPrintToken(const Token& token);

#endif