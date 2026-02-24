/* ************************************************************************** */
/*                                                                            */
/*   utils.hpp                                                                */
/*                                                                            */
/*   Shared utility functions used across the project.                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

namespace ws
{
    /* Logging helpers — all write to std::cerr */
    void log_info(const std::string &msg);
    void log_error(const std::string &msg);

    /* Convert an integer to a string (C++98 has no std::to_string) */
    std::string itos(int n);
}

#endif
