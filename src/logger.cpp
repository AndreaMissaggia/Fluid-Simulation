#include "logger.hpp"

void Logger::log(const std::string& message, const std::string& component, LogLevel level)
{
    std::string level_str {};

    switch(level)
    {
        case LogLevel::DEBUG:
        {
            level_str = "DEBUG";
            break;
        }

        case LogLevel::INFO:
        {
            level_str = "INFO";
            break;
        }

        case LogLevel::WARNING:
        {
            level_str = "WARNING";
            break;
        }

        case LogLevel::ERROR:
        {
            level_str = "ERROR";
            break;
        }

        default:
        {
            level_str = "UNKNOWN";
            break;
        }
    }

    std::time_t now { std::time(nullptr) };
    char time_str[128] {};
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::cout << "[" << time_str << "] [" << level_str << "] [" << component << "] " << message << std::endl;
}
