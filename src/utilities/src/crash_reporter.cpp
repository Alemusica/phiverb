#include "utilities/crash_reporter.h"

#if WAYVERB_ENABLE_CRASH_REPORTER

#include <chrono>
#include <csignal>
#include <ctime>
#include <execinfo.h>
#include <sys/stat.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <fcntl.h>

namespace util {
namespace crash {

namespace {
std::atomic<bool> g_installed{false};
std::atomic<bool> g_crashing{false};
std::mutex g_log_mutex;
std::string g_last_status;
std::string g_app_name;
std::string g_app_version;
std::string g_log_dir;

std::string now_string() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", std::localtime(&t));
    return buf;
}

bool ensure_dir(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    return ::mkdir(path.c_str(), 0755) == 0;
}

std::string default_log_dir() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::string base = std::string(home) + "/Library/Logs";
        (void)ensure_dir(base);
        std::string leaf = base + "/Wayverb";
        (void)ensure_dir(leaf);
        return leaf;
    }
    return "/tmp";
}

std::string make_log_path(const char* kind) {
    std::ostringstream oss;
    oss << (g_log_dir.empty() ? default_log_dir() : g_log_dir) << "/"
        << (g_app_name.empty() ? "wayverb" : g_app_name) << "-" << kind
        << "-" << now_string() << ".log";
    return oss.str();
}

void write_backtrace_fd(int fd) {
    void* frames[128];
    int n = ::backtrace(frames, 128);
#ifdef __APPLE__
    // best effort: backtrace_symbols_fd exists on macOS as well
    ::backtrace_symbols_fd(frames, n, fd);
#else
    ::backtrace_symbols_fd(frames, n, fd);
#endif
}

void signal_handler(int sig, siginfo_t*, void*) {
    if (g_crashing.exchange(true)) {
        _exit(128 + sig);
    }
    const std::string path = make_log_path("crash");
    int fd = ::creat(path.c_str(), 0644);
    if (fd >= 0) {
        std::string header = "[crash] signal=" + std::to_string(sig) + "\n";
        ::write(fd, header.data(), header.size());
        if (!g_last_status.empty()) {
            std::string s = std::string{"last_status="} + g_last_status + "\n";
            ::write(fd, s.data(), s.size());
        }
        std::string bt = "backtrace:\n";
        ::write(fd, bt.data(), bt.size());
        write_backtrace_fd(fd);
        ::close(fd);
    }
    // re-raise default to get system crash report too
    std::signal(sig, SIG_DFL);
    ::raise(sig);
}

void install_handlers() {
    struct sigaction sa{};
    sa.sa_sigaction = &signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);

    std::set_terminate([] {
        const std::string path = make_log_path("terminate");
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (out) {
            out << "[terminate] unhandled exception or std::terminate called\n";
            out << "last_status=" << g_last_status << "\n";
        }
        std::abort();
    });
}
}  // namespace

void reporter::install(const app_info& info) {
    if (g_installed.exchange(true)) return;
    g_app_name = info.app_name;
    g_app_version = info.app_version;
    const char* dir = std::getenv("WAYVERB_LOG_DIR");
    if (dir && *dir) g_log_dir = dir;
    install_handlers();
}

void reporter::set_status(const std::string& status) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_last_status = status;
}

void reporter::append_line(const std::string& line) {
    const std::string path = make_log_path("runtime");
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::ofstream out(path, std::ios::out | std::ios::app);
    if (out) {
        out << now_string() << " " << line << "\n";
    }
}

}  // namespace crash
}  // namespace util

#endif  // WAYVERB_ENABLE_CRASH_REPORTER
