#include "Logger.hpp"

Logger::Logger(const std::string& filename) : filename_(filename) {
    logging_thread_ = std::thread([this]() {
        while (true) {
            
            std::string message;
            {
                std::unique_lock<std::mutex> lk(mx_);
                cv_log.wait(lk, [this]{ return !logs_.empty(); });
                message = logs_[0];
                logs_.pop_front();
            }

            if (message == "END") {
                break;
            }

            auto now = std::chrono::system_clock::now();
            auto now_c = std::chrono::system_clock::to_time_t(now);

            // Convert std::chrono::system_clock::time_point to std::tm to extract date and time components
            std::tm timeinfo = *std::localtime(&now_c);

            // Format the timestamp
            std::ostringstream oss;
            oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
            std::string timestamp = oss.str();

            // Open file in append mode and write message with timestamp
            std::ofstream file(filename_, std::ios::app);
            if (file.is_open()) {
                file << "[" << timestamp << "] " << message << "\n";
                file.close();
            } else {
                std::cerr << "Failed to open log file" << std::endl;
            }
        }
    });
}

void Logger::Log(const std::string& message) {
    {
            std::lock_guard<std::mutex> lock(mx_);
            logs_.push_back(message);
            cv_log.notify_one();
    }
}

Logger::~Logger() {
    logging_thread_.join();
}