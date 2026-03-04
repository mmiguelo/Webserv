#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <string>
#include <vector>
#include "config/Token.hpp"

class Tokenizer {
	private:
	public:
		static std::vector<Token> tokenize(const std::string& input);
};

#endif