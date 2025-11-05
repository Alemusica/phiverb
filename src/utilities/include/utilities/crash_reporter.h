#pragma once

#include <atomic>
#include <string>

namespace util {
namespace crash {

#ifndef WAYVERB_ENABLE_CRASH_REPORTER
#define WAYVERB_ENABLE_CRASH_REPORTER 1
#endif

struct app_info final {
    std::string app_name;
    std::string app_version;
};

// Minimal crash reporter: installs signal and terminate handlers and writes
// a timestamped crash log with last known status and a best-effort backtrace.
class reporter final {
public:
    static void install(const app_info& info);
    static void set_status(const std::string& status);
    static void append_line(const std::string& line);

private:
    static void write_runtime_log(const char* prefix);
};

}  // namespace crash
}  // namespace util

