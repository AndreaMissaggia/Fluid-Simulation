#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <ctime>

#define LOG Logger::log

enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void log(const std::string& message, const std::string& component = " ? ", LogLevel level = LogLevel::INFO);
};

#endif // LOGGER_HPP
