#include "logger.h"

#include <sstream>

Logger::Logger() {
	std::stringstream stream;
	stream << TARGET_NAME << ".log";
	this->filestream.open(stream.str());
}

Logger::~Logger() {
	if (this->filestream.is_open()) {
		filestream.close();
	}
}

void Logger::writeLine() {
	if (this->filestream.is_open()) {
		filestream << std::endl;
	}
}

