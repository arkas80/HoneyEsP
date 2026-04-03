#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <iostream>

class Logger {
public:   
    Logger(const std::string &filename);
    ~Logger();
    void log(const std::string &message);

private:   
    std::ofstream logfile;
};

#endif // LOGGER_H
