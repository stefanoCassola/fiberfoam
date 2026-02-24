#pragma once

#include <chrono>
#include <string>
#include "Logger.h"

namespace fiberfoam
{

class Timer
{
public:
    explicit Timer(const std::string& label) : label_(label), start_(clock::now()) {}

    ~Timer()
    {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start_).count();
        Logger::info(label_ + " completed in " + std::to_string(elapsed) + " ms");
    }

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    double elapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start_).count()
               / 1000.0;
    }

private:
    using clock = std::chrono::high_resolution_clock;
    std::string label_;
    clock::time_point start_;
};

} // namespace fiberfoam
