#include "config/Tokenizer.hpp"
#include <iostream>
#include <cctype>


static bool checkNewLine(const std::string& fileContent, size_t& i, size_t& lineNum);
static bool checkWhitespace(const std::string& fileContent, size_t& i);
static bool checkComment(const std::string& fileContent, size_t& i);
static bool checkSymbol(const std::string& fileContent, size_t& i, std::vector<Token>& token_vector, size_t lineNum);
static void addWord(const std::string& fileContent, size_t& i, std::vector<Token>& token_vector, size_t lineNum);

/* Tokenizer::Tokenizer() {
	//std::cout << "Tokenizer constructor called" << std::endl;
}

Tokenizer::Tokenizer(const Tokenizer& other) {
	(void)other;
}

Tokenizer& Tokenizer::operator=(const Tokenizer& other) {
	if (this != &other)
		(void)other;
	return *this;
}

Tokenizer::~Tokenizer() {
	//std::cout << "Tokenizer destructor called" << std::endl;
} */

std::vector<Token> Tokenizer::tokenize(const std::string& fileContent) {
	std::vector<Token> tokens_vector;
	size_t lineNum = 1;
	size_t i = 0;

	while(i < fileContent.size()) {
		if (checkNewLine(fileContent, i, lineNum))
			continue;
		if (checkWhitespace(fileContent, i))
			continue;
		if (checkComment(fileContent, i))
			continue;
		if (checkSymbol(fileContent, i, tokens_vector, lineNum))
			continue;
		
		addWord(fileContent, i, tokens_vector, lineNum);
	}
	return tokens_vector;
}

static bool checkNewLine(const std::string& fileContent, size_t& i, size_t& lineNum) {
	if (i < fileContent.size() && fileContent[i] == '\n') {
		lineNum++;
		i++;
		return true;
	}
	return false;
}

static bool checkWhitespace(const std::string& fileContent, size_t& i) {
	if ( i < fileContent.size() && (fileContent[i] == ' ' || fileContent[i] == '\t' || fileContent[i] == '\r')) {
		i++;
		return true;
	}
	return false;
}

static bool checkComment(const std::string& fileContent, size_t& i) {
	if (i < fileContent.size() && fileContent[i] == '#') {
		while (i < fileContent.size() && fileContent[i] != '\n')
			i++;
		return true;
	}
	return false;
}

static bool checkSymbol(const std::string& fileContent, size_t& i, std::vector<Token>& token_vector, size_t lineNum) {
	if (i < fileContent.size() && fileContent[i] == '{') {
		token_vector.push_back(Token(LBRACE, "{", lineNum));
		i++;
		return true;
	}
	if (i < fileContent.size() && fileContent[i] == '}') {
		token_vector.push_back(Token(RBRACE, "}", lineNum));
		i++;
		return true;
	}
	if (i < fileContent.size() && fileContent[i] == ';') {
		token_vector.push_back(Token(SEMICOLON, ";", lineNum));
		i++;
		return true;
	}
	return false;
}

static void addWord(const std::string& fileContent, size_t& i, std::vector<Token>& token_vector, size_t lineNum) {
	std::string word;
	while (i < fileContent.size()) {
		if (fileContent[i] == '{' || fileContent[i] == '}' || fileContent[i] == ';' || std::isspace(fileContent[i]) || fileContent[i] == '#')
		break;
		word += fileContent[i];
		i++;
	}
	if (!word.empty())
	token_vector.push_back(Token(WORD, word, lineNum));
}