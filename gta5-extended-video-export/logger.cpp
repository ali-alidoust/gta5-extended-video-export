#include "logger.h"

#include <sstream>

Logger::Logger() {
	this->filestream.open(TARGET_NAME ".log");
}

Logger::~Logger() {
	if (this->filestream.is_open()) {
		std::lock_guard<std::mutex> guard(mtx);
		filestream.flush();
		filestream.close();
	}
}

void Logger::writeLine() {
	if (this->filestream.is_open()) {
		std::lock_guard<std::mutex> guard(mtx);
		filestream << std::endl;
	}
}

std::string Logger::getTimestamp() {
	char buffer[256];
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	strftime(buffer, 256, "[%Y-%m-%d %H:%M:%S] ", &timeinfo);
	return std::string(buffer);
}

