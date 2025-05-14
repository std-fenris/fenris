#ifndef FENRIS_COLORS_HPP
#define FENRIS_COLORS_HPP

#include <string>

namespace fenris {
namespace colors {

// Flag to control whether colors are used or not (for testing)
extern bool use_colors;

// Function to disable colors for testing
inline void disable_colors()
{
    use_colors = false;
}

// Function to enable colors
inline void enable_colors()
{
    use_colors = true;
}

// ANSI color escape codes
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string WHITE = "\033[37m";
const std::string BOLD = "\033[1m";

// Helper functions to colorize text
inline std::string error(const std::string &text)
{
    if (!use_colors)
        return text;
    return RED + text + RESET;
}

inline std::string warning(const std::string &text)
{
    if (!use_colors)
        return text;
    return YELLOW + text + RESET;
}

inline std::string success(const std::string &text)
{
    if (!use_colors)
        return text;
    return GREEN + text + RESET;
}

inline std::string info(const std::string &text)
{
    if (!use_colors)
        return text;
    return CYAN + text + RESET;
}

} // namespace colors
} // namespace fenris

#endif // FENRIS_COLORS_HPP
