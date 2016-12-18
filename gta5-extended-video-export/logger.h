#pragma once
#include <Windows.h>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <exception>
#include <sstream>
extern "C" {
	#include <libavutil\error.h>
}

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

enum LogLevel {
	LL_NON = 0,
	LL_ERR = 10,
	LL_WRN = 20,
	LL_NFO = 30,
	LL_DBG = 40,
	LL_TRC = 50
};

class Logger {
public:
	template <class T> void writeLine(const T& value) {
		if (this->filestream.is_open()) {
			{
				std::lock_guard<std::mutex> guard(mtx);
				filestream << object << std::endl;
			}
		}
	}

	template <typename... Targs> void write(const Targs&... args) {
		if (this->filestream.is_open()) {
			{
				std::lock_guard<std::mutex> guard(mtx);
				this->_write(args...);
			}
		}
	}

	template <typename T> static std::string hex(T number, int length) {
		std::stringstream stream;
		stream
			<< "0x"
			<< std::uppercase
			<< std::setfill('0')
			<< std::setw(length)
			<< std::hex
			<< number;

		return stream.str();
	}

	void writeLine();

	
	std::string getTimestamp();
	std::string getThreadId();
	std::string getLogLevelString(LogLevel level);
	LogLevel level = LL_NFO;
	
	static Logger& instance()
	{
		static Logger logger;
		return logger;
	}

	Logger(Logger const&) = delete;
	void operator=(Logger const&) = delete;

private:
	template <typename T, typename... Targs> void _write(const T& value, const Targs&... args) {
		if (this->filestream.is_open()) {
			//std::lock_guard<std::mutex> guard(mtx);
			filestream << value;
			this->_write(args...);
		}
	}

	template <typename T> void _write(const T& value) {
		if (this->filestream.is_open()) {
			//std::lock_guard<std::mutex> guard(mtx);
			filestream << value;
		}
	}

	Logger();
	~Logger();
	std::mutex mtx;
	std::ofstream filestream;
};

#ifndef LOG
#define LOG(ll, ...) if (ll <= Logger::instance().level) {Logger::instance().write(\
	Logger::instance().getTimestamp(),\
	" ",\
	Logger::instance().getLogLevelString(ll),\
	" ",\
	Logger::instance().getThreadId(),\
	" ",\
	__FILENAME__,\
	" (line ",\
	__LINE__,\
	"): ",\
	__VA_ARGS__,\
	std::endl<char,std::char_traits<char>>);\
}
#endif // ! LOG // Logger::instance().write(__LINE__); Logger::instance().writeLine(x);

#define PRE()  {LOG(LL_DBG, "Entering: ", __func__);}

#define POST() {LOG(LL_DBG, "Leaving: ",  __func__);}

#define LOG_CALL(ll, o) {LOG(ll, "Executing ",#o); o;}

#define LOG_CALL_BUT_NOT_INVOKE_IT(ll, o) {LOG(ll, "Executing ",#o);}

#define LOG_IF_NULL(o, m) if ((o) == NULL) {LOG(LL_WRN, m);}

#define LOG_IF_FAILED(o, m) {LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o); int __result = (int)o; if (FAILED(__result)) {LOG(LL_WRN, m, " ### error code: ", __result);}}

#define RET_IF_NULL(o, m, r) if ((o) == NULL) {LOG(LL_WRN, m); POST(); return r;}

#define RET_IF_FAILED(o, m, r) {LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o); int __result = (int)o; if (FAILED(__result)) {LOG(LL_WRN, m, " ### error code: ", __result); POST(); return r;}}

#define av_err2str2(errnum) char __av_error[AV_ERROR_MAX_STRING_SIZE]; av_make_error_string(__av_error, AV_ERROR_MAX_STRING_SIZE, errnum);

#define RET_IF_FAILED_AV(o, m, r) {LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o); int __result = (int)o; if (FAILED(__result)) {av_err2str2(__result); LOG(LL_WRN, m, " ### avcodec error : ", __av_error); POST(); return r;}}

#define LOG_IF_FAILED_AV(o, m) {LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o); int __result = (int)o; if (FAILED(__result)) {av_err2str2(__result); LOG(LL_WRN, m, " ### avcodec error : ", __av_error);}}

#define REQUIRE(o, m) {LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o); int __result = (int)o; if (FAILED(__result)) {LOG(LL_ERR, m, " ### error code: ", __result); throw std::runtime_error(m); }}

#define NOT_NULL(o, m) {if ((o) == NULL) {LOG(LL_ERR, m); throw std::runtime_error(m);}}
