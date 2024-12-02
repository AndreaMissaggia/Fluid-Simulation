#ifndef STOPWATCH_HPP
#define STOPWATCH_HPP

#include <chrono>
#include <iostream>
#include <string>

class Stopwatch {
public:
    Stopwatch();

    void start();
    void reset();
    void print();

    int64_t elapsed();
    std::string elapsed_as_string();

private:
    std::chrono::time_point<std::chrono::steady_clock> _start_point {};
};

#endif // STOP_WATCH_HPP
