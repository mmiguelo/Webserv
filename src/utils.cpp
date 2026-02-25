#include "utils.hpp"

void log_error(const std::string &message)
{
    std::cerr << COLOR_RED << "[ERROR]: " << COLOR_RESET << message << std::endl;
}

void log_info(const std::string &message)
{
    std::cout << COLOR_YELLOW << "[INFO]: " << COLOR_RESET << message << std::endl;
}
