#include "config/ConfigParser.hpp"
#include <stdexcept>

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
		throw std::runtime_error("Unexpected end of tokens");
	if (peek().type != type)
		throw std::runtime_error("Unexpected token");
	next();
}

std::vector<ServerConfig> ConfigParser::parse() {
	std::vector<ServerConfig> servers;
	while (!isEnd())
		servers.push_back(parseServerBlock());
	return servers;
}