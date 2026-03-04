#include <iostream>
#include <fstream>
#include <sstream>
#include "config/Tokenizer.hpp"
#include "config/Utils_config.hpp" // for debugPrintToken

// just a test file to check if the tokenizer is working correctly

/* c++ -Wall -Wextra -Werror \
-I includes \
main.cpp \
src/config/Tokenizer.cpp \
src/config/Utils_config.cpp \
-o test */

int main()
{
    std::ifstream file("test.conf");
    if (!file.is_open())
    {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::stringstream buffer; // create a stringstream to hold the file content
    buffer << file.rdbuf(); //outstream the file content into the stringstream
    std::string content = buffer.str(); // get the string from the stringstream

    Tokenizer tokenizer; // create an instance of the Tokenizer class
    std::vector<Token> tokens = tokenizer.tokenize(content); // call the tokenize method to get the vector of tokens

    for (size_t i = 0; i < tokens.size(); i++)
        debugPrintToken(tokens[i]); //print each token using the debugPrintToken function

    return 0;
}