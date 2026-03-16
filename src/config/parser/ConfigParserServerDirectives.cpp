#include "config/ConfigParser.hpp"
#include "config/UtilsConfig.hpp"
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

std::string ConfigParser::expectWord()
{
	if (isEnd() || peek().type != WORD)
		throw parseError("Expected value after directive");
	std::string value = peek().value;
	next();
	return value;
}

// Server Directives

void ConfigParser::parseListen(ServerConfig &serverBlock)
{
	std::string value = expectWord(); // avancamos do token "listen" para o valor da porta

	std::string host = "0.0.0.0";
	std::string port;
	size_t dividerPos = value.find(':');
	if (dividerPos != std::string::npos)
	{
		host = value.substr(0, dividerPos);
		port = value.substr(dividerPos + 1);

		if (host.empty())
			host = "0.0.0.0";
	}
	else
		port = value;

	if (!isNumber(port))
		throw parseError("Invalid port number: " + port);
	int portNum = std::atoi(port.c_str()); // convertemos a string para int
	serverBlock.setHost(host);			   // atribuimos o valor do host ao serverBlock
	serverBlock.setPort(portNum);		   // atribuimos o valor da porta ao serverBlock
	std::vector<int> ports = serverBlock.getPorts();
	expect(SEMICOLON); // verificamos se o próximo token é um ponto e vírgula
}

void ConfigParser::parseRoot(ServerConfig &serverBlock)
{
	serverBlock.setRoot(expectWord()); // avancamos do token "root" para o valor do root e atribuímos ao serverBlock
	expect(SEMICOLON);
}

void ConfigParser::parseServerName(ServerConfig &serverBlock)
{
	// o server name e um std::vector<std::string>, então podemos ter múltiplos nomes de servidor
	serverBlock.addServerName(expectWord()); // expectWord() ja verifica contra erros e avança para o próximo token. Entao e seguro adicionar
	while (!isEnd() && peek().type == WORD)	 // enquanto o próximo token for uma palavra. Seguimos
		serverBlock.addServerName(expectWord());
	expect(SEMICOLON);
}

void ConfigParser::parseMethods(ServerConfig &serverBlock)
{
	std::string method = expectWord();
	serverBlock.addMethod(method);
	while (!isEnd() && peek().type == WORD)
	{
		method = expectWord();
		serverBlock.addMethod(method);
	}
	expect(SEMICOLON);
}

void ConfigParser::parseClientMaxBodySize(ServerConfig &serverBlock)
{
	std::string sizeStr = expectWord(); // vamos receber o valor como string, exemplo "5M"
	size_t multiplier = 1;
	if (!sizeStr.empty() && std::isalpha(sizeStr[sizeStr.size() - 1]))
	{
		char unit = sizeStr[sizeStr.size() - 1];
		sizeStr.erase(sizeStr.size() - 1);
		switch (unit)
		{
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
	serverBlock.setClientMaxBodySize(sizeValue * multiplier); // atribuimos o valor ao serverBlock
	expect(SEMICOLON);
}

void ConfigParser::parseErrorPage(ServerConfig &serverBlock)
{
	std::string codeStr = expectWord(); // primeiro recebemos o código de erro como string
	int code = std::atoi(codeStr.c_str());
	if (code < 100 || code > 599)
		throw parseError("Invalid error code: " + codeStr);

	std::string path = expectWord();	  // depois recebemos o path
	serverBlock.addErrorPage(code, path); // atribuimos o código e o path ao map de error_page
	expect(SEMICOLON);
}