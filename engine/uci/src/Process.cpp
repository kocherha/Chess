#include "chess/Process.h"

#if defined(_WIN32)

namespace chess {

Process::~Process() = default;

bool Process::start(const std::string&, std::string* error) {
    if (error != nullptr)
        *error = "starting an engine process is not implemented on Windows yet";
    return false;
}
bool Process::writeLine(std::string_view) { return false; }
bool Process::readLine(std::string&) { return false; }
void Process::closeInput() {}
void Process::stop(int) {}

}  // namespace chess

#else  // POSIX: macOS, Linux

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <thread>

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// The environment to hand to the child. On macOS the `environ` global is not
// something a library may reference directly; _NSGetEnviron() is the
// supported way to reach it, and works from an executable too.
#if defined(__APPLE__)
#    include <crt_externs.h>
#    define CHESS_ENVIRON (*_NSGetEnviron())
#else
extern char** environ;
#    define CHESS_ENVIRON environ
#endif

namespace chess {

namespace {

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void setCloseOnExec(int fd) {
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags >= 0)
        ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

}  // namespace

Process::~Process() {
    stop(0);
    closeFd(_fromChild);
}

bool Process::start(const std::string& path, std::string* error) {
    const auto fail = [error](const std::string& what) {
        if (error != nullptr)
            *error = what;
        return false;
    };

    if (_started)
        return fail("already started");
    if (::access(path.c_str(), X_OK) != 0)
        return fail("not an executable: " + path);

    // Writing to a program that has died raises SIGPIPE, and the default
    // action for SIGPIPE is to kill the writer. An opponent that crashed must
    // not take the game down with it: with the signal ignored, the write
    // simply fails with EPIPE and writeLine() reports it.
    std::signal(SIGPIPE, SIG_IGN);

    int toChild[2]   = {-1, -1};
    int fromChild[2] = {-1, -1};
    if (::pipe(toChild) != 0)
        return fail(std::string("pipe: ") + std::strerror(errno));
    if (::pipe(fromChild) != 0) {
        closeFd(toChild[0]);
        closeFd(toChild[1]);
        return fail(std::string("pipe: ") + std::strerror(errno));
    }

    // Our ends must not leak into this child or any later one: an engine that
    // inherited the write end of its own input would never see end-of-file.
    setCloseOnExec(toChild[1]);
    setCloseOnExec(fromChild[0]);

    // posix_spawn rather than fork: this is called from a program with a
    // render thread and a GPU driver loaded, and forking that is asking for
    // trouble on macOS.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, toChild[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fromChild[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, toChild[0]);
    posix_spawn_file_actions_addclose(&actions, fromChild[1]);

    char* const argv[] = {const_cast<char*>(path.c_str()), nullptr};
    pid_t pid          = -1;
    const int result   = ::posix_spawn(&pid, path.c_str(), &actions, nullptr, argv, CHESS_ENVIRON);
    posix_spawn_file_actions_destroy(&actions);

    closeFd(toChild[0]);
    closeFd(fromChild[1]);

    if (result != 0) {
        closeFd(toChild[1]);
        closeFd(fromChild[0]);
        return fail("could not start " + path + ": " + std::strerror(result));
    }

    _pid       = pid;
    _toChild   = toChild[1];
    _fromChild = fromChild[0];
    _started   = true;
    return true;
}

bool Process::writeLine(std::string_view line) {
    if (_toChild < 0)
        return false;

    std::string out(line);
    out += '\n';

    std::size_t sent = 0;
    while (sent < out.size()) {
        const ssize_t n = ::write(_toChild, out.data() + sent, out.size() - sent);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool Process::readLine(std::string& line) {
    for (;;) {
        const std::size_t end = _buffer.find('\n');
        if (end != std::string::npos) {
            line.assign(_buffer, 0, end);
            _buffer.erase(0, end + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return true;
        }

        if (_fromChild < 0)
            return false;

        char chunk[4096];
        const ssize_t n = ::read(_fromChild, chunk, sizeof(chunk));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            // End of output. A last line without a newline is still a line.
            if (_buffer.empty())
                return false;
            line.swap(_buffer);
            _buffer.clear();
            return true;
        }
        _buffer.append(chunk, static_cast<std::size_t>(n));
    }
}

void Process::closeInput() {
    closeFd(_toChild);
}

void Process::stop(int graceMs) {
    closeInput();

    if (_pid > 0) {
        int  status = 0;
        bool exited = false;
        for (int waited = 0; waited <= graceMs; waited += 10) {
            const pid_t r = ::waitpid(_pid, &status, WNOHANG);
            if (r == _pid || (r < 0 && errno != EINTR)) {
                exited = true;
                break;
            }
            if (waited < graceMs)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!exited) {
            ::kill(_pid, SIGKILL);
            while (::waitpid(_pid, &status, 0) < 0 && errno == EINTR) {
            }
        }
        _pid = -1;
    }

    // Our reading end stays open until the destructor: a thread blocked in
    // readLine() is woken by the end-of-file the dead child leaves behind,
    // not by a descriptor closed under it.
}

}  // namespace chess

#endif
