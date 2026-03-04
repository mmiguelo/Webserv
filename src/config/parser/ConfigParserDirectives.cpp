#include "config/ConfigParser.hpp"
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

std::string ConfigParser::expectWord() {
	if (isEnd() || peek().type != WORD)
		throw parseError("Expected value after directive");
	std::string value = peek().value;
	next();
	return value;
}

//Server Directives

void ConfigParser::parseListen(ServerConfig& serverBlock) {
	std::string port = expectWord(); //avancamos do token "listen" para o valor da porta
	int portNum = std::atoi(port.c_str()); //convertemos a string para int
	if (portNum <= 0 || portNum > 65535)
		throw parseError("Invalid port number: " + port);
	serverBlock.port = portNum; //atribuimos o valor da porta ao serverBlock
	expect(SEMICOLON); //verificamos se o próximo token é um ponto e vírgula
}

void ConfigParser::parseRoot(ServerConfig& serverBlock) {
	serverBlock.root = expectWord(); //avancamos do token "root" para o valor do root e atribuímos ao serverBlock
	expect(SEMICOLON);
}

void ConfigParser::parseServerName(ServerConfig& serverBlock){
	//o server name e um std::vector<std::string>, então podemos ter múltiplos nomes de servidor
	serverBlock.server_name.push_back(expectWord()); //expectWord() ja verifica contra erros e avança para o próximo token. Entao e seguro adicionar
	while (!isEnd() && peek().type == WORD) //enquanto o próximo token for uma palavra. Seguimos
		serverBlock.server_name.push_back(expectWord());
	expect(SEMICOLON);
}

void ConfigParser::parseMethods(ServerConfig& serverBlock) {
	//o methods e um std::vector<std::string>, ent a funcao sera semelhante
	serverBlock.methods.push_back(expectWord());
	while (!isEnd() && peek().type == WORD)
		serverBlock.methods.push_back(expectWord());
	expect(SEMICOLON);
}

void ConfigParser::parseClientMaxBodySize(ServerConfig& serverBlock) {
	std::string sizeStr = expectWord(); //vamos receber o valor como string, exemplo "5M"
	size_t multiplier = 1;
	if (!sizeStr.empty() && isalpha(sizeStr.back())) {
		char unit = sizeStr.back();
		sizeStr.pop_back();
		switch (unit) {
			case 'K':
				multiplier *= 1024;
				break;
			case 'M':
				multiplier *= 1024 * 1024;
				break;
			case 'G':
				multiplier *= 1024 * 1024 * 1024;
				break;
			default:
				throw parseError("Invalid size unit: " + std::string(1, unit));
		}
	}
	if (sizeStr.empty())
		throw parseError("Size value is missing");
	size_t sizeValue = std::atoi(sizeStr.c_str());
	if (sizeValue <= 0)
		throw parseError("Invalid size value: " + sizeStr);
	serverBlock.client_max_body_size = sizeValue * multiplier; //atribuimos o valor ao serverBlock
	expect(SEMICOLON);
}

void ConfigParser::parseErrorPage(ServerConfig& serverBlock) {
	std::string codeStr = expectWord(); //primeiro recebemos o código de erro como string
	int code = std::atoi(codeStr.c_str());
	if (code < 100 || code > 599)
		throw parseError("Invalid error code: " + codeStr);
	std::string path = expectWord(); //depois recebemos o path
	serverBlock.error_page[code] = path; //atribuimos o código e o path ao map de error_page
	if (serverBlock.error_page.count(code) > 1)
		throw parseError("Duplicate error code: " + codeStr);
	expect(SEMICOLON);
}