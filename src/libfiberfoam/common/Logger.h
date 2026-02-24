#pragma once

#include <iostream>
#include <string>

namespace fiberfoam
{

class Logger
{
public:
    enum class Level
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    static void setLevel(Level level) { minLevel() = level; }

    static void debug(const std::string& msg)
    {
        if (minLevel() <= Level::Debug)
            std::cout << "[DEBUG] " << msg << std::endl;
    }

    static void info(const std::string& msg)
    {
        if (minLevel() <= Level::Info)
            std::cout << "[INFO] " << msg << std::endl;
    }

    static void warning(const std::string& msg)
    {
        if (minLevel() <= Level::Warning)
            std::cerr << "[WARN] " << msg << std::endl;
    }

    static void error(const std::string& msg)
    {
        if (minLevel() <= Level::Error)
            std::cerr << "[ERROR] " << msg << std::endl;
    }

private:
    static Level& minLevel()
    {
        static Level level = Level::Info;
        return level;
    }
};

} // namespace fiberfoam
