#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <deque>
#include <thread>
#include <condition_variable>

class Logger {
public:
    Logger(const std::string& filename = "log.txt");

    void Log(const std::string& message);
    ~Logger();

private:
    std::thread logging_thread_;
    std::deque<std::string> logs_;
    std::string filename_;
    std::mutex mx_;
    std::condition_variable cv_log;
};

#endif // LOGGER_HPP