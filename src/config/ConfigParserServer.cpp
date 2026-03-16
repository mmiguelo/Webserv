#include "ConfigParser.hpp"
#include <iostream>
#include <stdexcept>


ServerConfig ConfigParser::parseServerBlock() {
	ServerConfig serverBlock;

	if (!matchWord("server"))
		throw parseError("Expected 'server'");
	expect(LBRACE);
	
	while(!isEnd() && peek().type != RBRACE) {
		if (matchWord("listen"))
			parseListen(serverBlock);
		else if (matchWord("root"))
			parseRoot(serverBlock);
		else if (matchWord("server_name"))
			parseServerName(serverBlock);
		else if (matchWord("methods"))
			parseMethods(serverBlock);
		else if (matchWord("client_max_body_size"))
			parseClientMaxBodySize(serverBlock);
		else if (matchWord("error_page"))
			parseErrorPage(serverBlock);
		else if (matchWord("location"))
			parseLocation(serverBlock);
		else
			throw parseError("Unexpected directive in server block: " + peek().value);
	}
	expect(RBRACE);
	return serverBlock;
}