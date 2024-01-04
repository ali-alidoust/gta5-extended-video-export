#include "logger.h"

#include "util.h"

#include <iostream>

Logger::Logger() { ensureStream(); }

Logger::~Logger() {
    try {
        if (this->filestream.is_open()) {
            std::lock_guard<std::mutex> guard(mtx);
            filestream.flush();
            filestream.close();
        }
    } catch (...) {
        // Do nothing
    }
}

bool Logger::ensureStream() {
    if (this->filestream.is_open()) {
        return true;
    }
    this->filestream.open(AsiPath() + "\\EVE\\" TARGET_NAME ".log");
    if (this->filestream.is_open()) {
        std::lock_guard<std::mutex> guard(mtx);
        filestream << "Logger initialized." << "\r\n";
        return true;
    }
    return false;
}

void Logger::writeLine() {
    if (this->ensureStream()) {
        std::lock_guard<std::mutex> guard(mtx);
        filestream << "\r\n";
        filestream.flush();
    }
}

std::string Logger::getTimestamp() {
    char buffer[256];
    time_t rawtime;
    struct tm timeinfo;
    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);
    strftime(buffer, 256, "[%Y-%m-%d %H:%M:%S]", &timeinfo);
    return buffer;
}

std::string Logger::getThreadId() {
    std::stringstream stream;
    stream << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << std::this_thread::get_id()
           << std::dec << std::setw(0) << std::setfill(' ');
    return stream.str();
}

std::string Logger::getLogLevelString(const LogLevel level) {
    switch (level) {
    case LL_NON:
        return "NON";
    case LL_ERR:
        return "ERR";
    case LL_WRN:
        return "WRN";
    case LL_NFO:
        return "NFO";
    case LL_DBG:
        return "DBG";
    case LL_TRC:
        return "TRC";
    }
    return "UNK";
}