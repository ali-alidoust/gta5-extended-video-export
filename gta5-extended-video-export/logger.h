#pragma once
#include <Windows.h>
#include <fstream>
#include <mutex>
extern "C" {
	#include <libavutil\error.h>
}
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

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

	template <typename T> void write(const T& value) {
		if (this->filestream.is_open()) {
			{
				std::lock_guard<std::mutex> guard(mtx);
				filestream << value;
			}
		}
	}

	template <typename T, typename... Targs> void write(const T& value, const Targs&... args) {
		if (this->filestream.is_open()) {
			{
				std::lock_guard<std::mutex> guard(mtx);
				filestream << value;
			}
			this->write(args...);
		}
	}

	void writeLine();
	
	static Logger& instance()
	{
		static Logger logger;
		return logger;
	}

	Logger(Logger const&) = delete;
	void operator=(Logger const&) = delete;

private:
	Logger();
	~Logger();
	std::mutex mtx;
	std::ofstream filestream;
};

#ifndef LOG
#define LOG(...) Logger::instance().write(__FILENAME__, " (line ", __LINE__, "): ", __VA_ARGS__, std::endl<char,std::char_traits<char>>);
#endif // ! LOG // Logger::instance().write(__LINE__); Logger::instance().writeLine(x);

#ifndef LOG_CALL
#define LOG_CALL(o) {LOG("Executing ",#o);}
#endif

#ifndef LOG_IF_NULL
#define LOG_IF_NULL(o, m) if ((o) == NULL) {LOG(m);}
#endif

#ifndef LOG_IF_FAILED
#define LOG_IF_FAILED(o, m) {LOG_CALL(o); int __result = (int)o; if (FAILED(__result)) {LOG(m, " ### error code: ", __result);}}
#endif

#ifndef RET_IF_NULL
#define RET_IF_NULL(o, m, r) if ((o) == NULL) {LOG(m); return r;}
#endif

#ifndef RET_IF_FAILED
#define RET_IF_FAILED(o, m, r) {LOG_CALL(o); int __result = (int)o; if (FAILED(__result)) {LOG(m, " ### error code: ", __result); return r;}}
#endif

// TODO: replace this with something less stupid.
#define av_err2str2(errnum) char __av_error[AV_ERROR_MAX_STRING_SIZE]; av_make_error_string(__av_error, AV_ERROR_MAX_STRING_SIZE, errnum);

#ifndef RET_IF_FAILED_AV
#define RET_IF_FAILED_AV(o, m, r) {LOG_CALL(o); int __result = (int)o; if (FAILED(__result)) {av_err2str2(__result); LOG(m, " ### avcodec error : ", __av_error); return r;}}
#endif

#ifndef LOG_IF_FAILED_AV
#define LOG_IF_FAILED_AV(o, m) {LOG_CALL(o); int __result = (int)o; if (FAILED(__result)) {av_err2str2(__result); LOG(m, " ### avcodec error : ", __av_error);}}
#endif

