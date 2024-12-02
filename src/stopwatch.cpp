#include "stopwatch.hpp"

Stopwatch::Stopwatch() {}

void Stopwatch::start()
{
    _start_point = std::chrono::steady_clock::now();
}

void Stopwatch::reset()
{
    _start_point = std::chrono::time_point<std::chrono::steady_clock> {};
}

int64_t Stopwatch::elapsed()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _start_point).count();
}

std::string Stopwatch::elapsed_as_string()
{
    return std::to_string(elapsed()) + "ms";
}

void Stopwatch::print()
{
    std::cout << elapsed();
}
