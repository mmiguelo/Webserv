/* ************************************************************************** */
/*                                                                            */
/*   utils.cpp                                                                */
/*                                                                            */
/* ************************************************************************** */

#include "utils.hpp"
#include <iostream>
#include <sstream>

namespace ws
{

    void log_info(const std::string &msg)
    {
        std::cerr << "[INFO]  " << msg << std::endl;
    }

    void log_error(const std::string &msg)
    {
        std::cerr << "[ERROR] " << msg << std::endl;
    }

    std::string itos(int n)
    {
        std::ostringstream oss;
        oss << n;
        return oss.str();
    }

} // namespace ws
