#include "config/ConfigParser.hpp"
#include <stdexcept>
#include <sstream>

ConfigParser::ConfigParser(const std::vector<Token>& tokens) : _tokens(tokens), _pos(0) {} //iniciamos a referencia do vector de tokens e o index em 0

const Token& ConfigParser::peek() const{
	return _tokens[_pos];
}

const Token& ConfigParser::next(){
	return _tokens[_pos++];
}

bool ConfigParser::isEnd() const {
	return _pos >= _tokens.size();
}

bool ConfigParser::matchWord(const std::string& word) {
	if (!isEnd() && peek().type == WORD && peek().value == word) {
		next();
		return true;
	}
	return false;
}

void ConfigParser::expect(TokenType type)
{
	if (isEnd())
		throw parseError("Unexpected end of file");
	const Token& token = peek();
	if (token.type != type)
		throw parseError("Expected '" + tokenTypeToString(type) +"' but got '" + token.value + "'");
	next();
}

std::vector<ServerConfig> ConfigParser::parse() {
	std::vector<ServerConfig> servers;
	while (!isEnd())
		servers.push_back(parseServerBlock());
	return servers;
}

std::runtime_error ConfigParser::parseError(const std::string& message) const
{
	if (isEnd())
		return std::runtime_error("Config error at end of file: " + message);

	const Token& token = _tokens[_pos];
	std::stringstream ss;
	ss << token.lineNum;
	return std::runtime_error(
		"Config error at line " + ss.str() + ": " + message
	);
}

std::string ConfigParser::tokenTypeToString(TokenType type) const
{
	switch (type) {
		case WORD: return "WORD";
		case SEMICOLON: return "SEMICOLON";
		case RBRACE: return "RBRACE";
		case LBRACE: return "LBRACE";
		default: return "UNKNOWN";
	}
}

std::string ConfigParser::numberToString(size_t number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}