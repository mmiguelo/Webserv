#ifndef TOKEN_HPP
#define TOKEN_HPP

#include <string>

enum TokenType {
    WORD, // sequencia de caracteres sem espaços ou caracteres especiais "ola" "adeus" "9999" 
    LBRACE, // "{"
    RBRACE, // "}"
    SEMICOLON, // ";"
}; //usamos enums para definir os tipos de tokens

struct Token{
	TokenType type; // tipo do token, por exemplo, WORD, NUMBER, LBRACE, RBRACE, SEMICOLON
	std::string value; // valor do token, por exemplo, "ola", "12345", "{", "}", ";"
	size_t lineNum; // linha onde o token foi encontrado

	Token() : type(WORD), value(""), lineNum(0) {}

	Token(TokenType type, const std::string& value, size_t lineNum)
		: type(type), value(value), lineNum(lineNum) {} //parameterized constructor
};

#endif