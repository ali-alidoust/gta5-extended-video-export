// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
#pragma once

#define NOMINMAX
#include <Windows.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>

#define __CUSTOM_FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#pragma warning(push, 0)
enum LogLevel { LL_NON = 0, LL_ERR = 10, LL_WRN = 20, LL_NFO = 30, LL_DBG = 40, LL_TRC = 50 };
#pragma warning(pop)

class Logger {
  public:
    template <class T> void writeLine(const T& value) {
        if (this->ensureStream()) {
            {
                std::lock_guard guard(mtx);
                filestream << value << "\n";
            }
        }
    }

    template <typename... Targs> void write(const Targs&... args) {
        if (this->ensureStream()) {
            {
                std::lock_guard guard(mtx);
                this->_write(args...);
            }
        }
    }

    template <typename T> static std::string hex(const T number, const int length) {
        std::stringstream stream;
        stream << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << number;

        return stream.str();
    }

    void writeLine();

    static std::string getTimestamp();
    static std::string getThreadId();
    static std::string getLogLevelString(LogLevel level);
    LogLevel level = LL_NFO;

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    Logger(Logger const&) = delete;
    void operator=(Logger const&) = delete;

  private:
    template <typename T, typename... Targs> void _write(const T& value, const Targs&... args) {
        if (this->ensureStream()) {
            filestream << value;
            this->_write(args...);
            filestream.flush();
        }
    }

    template <typename T> void _write(const T& value) {
        if (this->ensureStream()) {
            filestream << value;
        }
    }

    Logger();
    ~Logger();
    std::mutex mtx;
    std::ofstream filestream;

    bool ensureStream();
};

#define REQUIRE_SEMICOLON                                                                                              \
    static_assert(true, "semi-colon required after this macro, per zwhconst's excellent suggestion at "                \
                        "https://stackoverflow.com/a/59153563/1261599")

#ifndef LOG
#define LOG(ll, ...)                                                                                                   \
    if (ll <= ::Logger::instance().level) {                                                                              \
        ::Logger::instance().write(::Logger::instance().getTimestamp(), " ", ::Logger::instance().getLogLevelString(ll),     \
                                 " ", ::Logger::instance().getThreadId(), " ", __CUSTOM_FILENAME__, " (line ", __LINE__, \
                                 "): ", __VA_ARGS__, "\n");                         \
    }                                                                                                                  \
    REQUIRE_SEMICOLON
#endif // ! LOG // Logger::instance().write(__LINE__); Logger::instance().writeLine(x);

#define PRE()                                                                                                          \
    { LOG(LL_TRC, "Entering: ", __FUNCSIG__); }                                                                        \
    REQUIRE_SEMICOLON

#define POST()                                                                                                         \
    { LOG(LL_TRC, "Leaving: ", __FUNCSIG__); }                                                                         \
    REQUIRE_SEMICOLON

#define LOG_CALL(ll, o)                                                                                                \
    {                                                                                                                  \
        LOG(ll, "Executing ", #o);                                                                                     \
        o;                                                                                                             \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define LOG_CALL_BUT_NOT_INVOKE_IT(ll, o)                                                                              \
    { LOG(ll, "Executing ", #o); }                                                                                     \
    REQUIRE_SEMICOLON

#define LOG_IF_NULL(o, m)                                                                                              \
    if ((o) == NULL) {                                                                                                 \
        LOG(LL_WRN, m);                                                                                                \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define LOG_IF_FAILED(o, m)                                                                                            \
    {                                                                                                                  \
        LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o);                                                                         \
        int __result = (int)o;                                                                                         \
        if (FAILED(__result)) {                                                                                        \
            LOG(LL_WRN, m, " ### error code: ", __result);                                                             \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define RET_IF_NULL(o, m, r)                                                                                           \
    if ((o) == NULL) {                                                                                                 \
        LOG(LL_WRN, m);                                                                                                \
        POST();                                                                                                        \
        return r;                                                                                                      \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define RET_IF_FAILED(o, m, r)                                                                                         \
    {                                                                                                                  \
        LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o);                                                                         \
        int __result = (int)o;                                                                                         \
        if (FAILED(__result)) {                                                                                        \
            LOG(LL_WRN, m, " ### error code: ", __result);                                                             \
            POST();                                                                                                    \
            return r;                                                                                                  \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define av_err2str2(errnum)                                                                                            \
    char __av_error[AV_ERROR_MAX_STRING_SIZE];                                                                         \
    av_make_error_string(__av_error, AV_ERROR_MAX_STRING_SIZE, errnum);                                                \
    REQUIRE_SEMICOLON

#define RET_IF_FAILED_AV(o, m, r)                                                                                      \
    {                                                                                                                  \
        LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o);                                                                         \
        int __result = (int)o;                                                                                         \
        if (FAILED(__result)) {                                                                                        \
            av_err2str2(__result);                                                                                     \
            LOG(LL_WRN, m, " ### avcodec error : ", __av_error);                                                       \
            POST();                                                                                                    \
            return r;                                                                                                  \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define LOG_IF_FAILED_AV(o, m)                                                                                         \
    {                                                                                                                  \
        LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o);                                                                         \
        int __result = (int)o;                                                                                         \
        if (FAILED(__result)) {                                                                                        \
            av_err2str2(__result);                                                                                     \
            LOG(LL_WRN, m, " ### avcodec error : ", __av_error);                                                       \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define REQUIRE(o, m)                                                                                                  \
    {                                                                                                                  \
        LOG_CALL_BUT_NOT_INVOKE_IT(LL_DBG, o);                                                                         \
        int __result = (int)o;                                                                                         \
        if (FAILED(__result)) {                                                                                        \
            LOG(LL_ERR, m, " ### error code: ", __result);                                                             \
            throw std::runtime_error(m);                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define NOT_NULL(o, m)                                                                                                 \
    {                                                                                                                  \
        if ((o) == NULL) {                                                                                             \
            LOG(LL_ERR, m);                                                                                            \
            throw std::runtime_error(m);                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define ASSERT_RUNTIME(c, m)                                                                                           \
    {                                                                                                                  \
        if (!(c)) {                                                                                                    \
            LOG(LL_ERR, m);                                                                                            \
            throw std::runtime_error(m);                                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    REQUIRE_SEMICOLON

#define TRY(o)                                                                                                         \
    [&] {                                                                                                              \
        try {                                                                                                          \
            o();                                                                                                       \
            return S_OK;                                                                                               \
        } catch (std::exception & ex) {                                                                                \
            LOG(LL_ERR, ex.what());                                                                                    \
            return E_FAIL;                                                                                             \
        }                                                                                                              \
    }()
