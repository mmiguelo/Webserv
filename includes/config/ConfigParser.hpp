#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <vector>
#include <string>
#include <stdexcept>
#include "config/Tokenizer.hpp"
#include "config/ServerConfig.hpp"
#include "config/LocationConfig.hpp"

class ConfigParser {
	private:
		const std::vector<Token> &_tokens; // referencia do vector de tokens passados pelos tokenizer, iniciado no constructor
		size_t _pos;					   // indice para vector de tokens

		// Server Directives
		ServerConfig parseServerBlock(); // função para fazer parse da class server
		void parseListen(ServerConfig &serverBlock);
		void parseRoot(ServerConfig &serverBlock);
		void parseServerName(ServerConfig &serverBlock);
		void parseMethods(ServerConfig &serverBlock);
		void parseClientMaxBodySize(ServerConfig &serverBlock);
		void parseErrorPage(ServerConfig &serverBlock);
		void parseLocation(ServerConfig &serverBlock);
		void parseLargeHeaderBuffers(ServerConfig &serverBlock);

		// Location Directives
		void parseLocationRoot(LocationConfig &location);
		void parseUploadDir(LocationConfig &location);
		void parseAutoindex(LocationConfig &location);
		void parseLocationMethods(LocationConfig &location);
		void parseLocationClientMaxBodySize(LocationConfig &location);
		void parseCgiExt(LocationConfig &location);
		void parseReturn(LocationConfig &location);
		void parseLocationIndex(LocationConfig &location);
		void parseLocationError(LocationConfig &location);

		// aux functions
		const Token &peek() const;				 // retorna o token atual
		const Token &next();					 // avança para o próximo token
		bool isEnd() const;						 // verificamos se chegamos ao fim do vector
		void expect(TokenType type);			 // verificamos se o token atual é do type esperado
		bool matchWord(const std::string &word); // verificamos se o token atual é uma palavra específica
		std::string expectWord();
		std::string tokenTypeToString(TokenType type) const; // função para converter o enum TokenType em string
		std::string numberToString(size_t number);

		// add runtime error
		std::runtime_error parseError(const std::string &message) const; // função para lançar exceção com mensagem de erro

	public:
		ConfigParser(const std::vector<Token> &tokens);
		void parse(std::map<int, std::vector<ServerConfig> > &servers);
};

#endif