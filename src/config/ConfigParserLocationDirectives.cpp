#include "ConfigParser.hpp"
#include "utils.hpp"
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

// Location Directives

void ConfigParser::parseLocationRoot(LocationConfig &location)
{
	location.root = toAbsolutePath(expectWord());
	expect(SEMICOLON);
}

void ConfigParser::parseUploadDir(LocationConfig &location)
{
	//TODO: dont know if we should add toAbsolutePath here
	location.upload_dir = toAbsolutePath(expectWord());
	expect(SEMICOLON);
}

void ConfigParser::parseAutoindex(LocationConfig &location)
{
	std::string value = expectWord();

	if (value == "on")
		location.autoindex = true;
	else if (value == "off")
		location.autoindex = false;
	else
		throw parseError("autoindex must be 'on' | 'off'");
	expect(SEMICOLON);
}

void ConfigParser::parseLocationMethods(LocationConfig &location)
{
	location.methods.push_back(expectWord());
	while (!isEnd() && peek().type == WORD)
		location.methods.push_back(expectWord());

	expect(SEMICOLON);
}

void ConfigParser::parseLocationClientMaxBodySize(LocationConfig &location)
{
	std::string sizeStr = expectWord();
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
	location.client_max_body_size = sizeValue * multiplier;
	location.has_client_max_body_size = true;
	expect(SEMICOLON);
}

void ConfigParser::parseCgiExt(LocationConfig &location)
{
	std::string extension = expectWord();
	std::string interpreter = expectWord();
	if (location.cgi_ext.count(extension) > 0)
		throw parseError("Duplicate CGI extension: " + extension);
	location.cgi_ext[extension] = interpreter;
	expect(SEMICOLON);
}

void ConfigParser::parseReturn(LocationConfig &location)
{
	// return 301 http://example.com/;
	// or like this: return 404;

	std::string codeStr = expectWord();
	int code = std::atoi(codeStr.c_str());
	if (code < 100 || code > 599)
		throw parseError("Invalid return code: " + codeStr);
	location.redirect_code = code;
	location.has_redirect = true;
	if (!isEnd() && peek().type == WORD)
		location.redirect_url = expectWord();
	expect(SEMICOLON);
}

void ConfigParser::parseLocationIndex(LocationConfig &location)
{
	location.index.push_back(expectWord());
	while (!isEnd() && peek().type == WORD)
		location.index.push_back(expectWord());

	expect(SEMICOLON);
}

void ConfigParser::parseLocationError(LocationConfig &location) {
	std::vector<int> codes;

	while (peek().type == WORD && isNumber(peek().value)) {
		std::string codeStr = expectWord();
		int code = std::atoi(codeStr.c_str());
		if (code < 100 || code > 599)
			throw parseError("Invalid error code: " + codeStr);
		codes.push_back(code);
	}

	std::string path = expectWord(); //depois recebemos o path
	for (size_t i = 0; i < codes.size(); ++i)
		location.error_page[codes[i]] = path; //atribuimos o código e o path ao serverBlock
	expect(SEMICOLON);
}