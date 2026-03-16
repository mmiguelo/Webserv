#include "ConfigParser.hpp"
#include "utils.hpp"
#include <iostream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>

//Location Directives

void ConfigParser::parseLocationRoot(LocationConfig& location)
{
	location.root = toAbsolutePath(expectWord());
	expect(SEMICOLON);
}

void ConfigParser::parseUploadDir(LocationConfig& location)
{
	location.upload_dir = expectWord();
	expect(SEMICOLON);
}

void ConfigParser::parseAutoindex(LocationConfig& location)
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

void ConfigParser::parseLocationMethods(LocationConfig& location)
{
	location.methods.push_back(expectWord());
	while (!isEnd() && peek().type == WORD)
		location.methods.push_back(expectWord());

	expect(SEMICOLON);
}

void ConfigParser::parseCgiExt(LocationConfig& location)
{
	std::string extension = expectWord();
	std::string interpreter = expectWord();
	if (location.cgi_ext.count(extension) > 0)
		throw parseError("Duplicate CGI extension: " + extension);
	location.cgi_ext[extension] = interpreter;
	expect(SEMICOLON);
}

void ConfigParser::parseReturn(LocationConfig& location)
{
	//return 301 http://example.com/;
	//or like this: return 404;

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

void ConfigParser::parseLocationIndex(LocationConfig& location)
{
	location.index.push_back(expectWord());
	while (!isEnd() && peek().type == WORD)
		location.index.push_back(expectWord());

	expect(SEMICOLON);
}