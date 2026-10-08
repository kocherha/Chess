// A child process we can write lines to and read lines from.
//
// That is all a UCI engine is from the outside: a program that takes commands
// on its standard input and answers on its standard output. This class is the
// only place that knows how the operating system starts one.
#pragma once

#include <string>
#include <string_view>

namespace chess {

class Process {
public:
    Process() = default;
    ~Process();

    Process(const Process&)            = delete;
    Process& operator=(const Process&) = delete;

    // Starts the program at `path` with no arguments. Returns false, with a
    // reason, if it could not be started.
    bool start(const std::string& path, std::string* error = nullptr);

    bool started() const { return _started; }

    // Sends one line; the newline is added here. False once the other end has
    // gone away. Safe to call from one thread while another is in readLine().
    bool writeLine(std::string_view line);

    // Waits for the next line of output and returns it without its line ending.
    // False when the program has closed its output — normally because it has
    // exited.
    bool readLine(std::string& line);

    // Closes our end of its input — the polite way to say "we are done": every
    // UCI engine exits when its standard input ends. writeLine() fails from
    // here on.
    void closeInput();

    // Waits `graceMs` for it to exit on its own and kills it if it has not.
    // Also closes the input, if that has not been done. Safe to call twice.
    void stop(int graceMs = 500);

private:
    bool _started = false;

#if defined(_WIN32)
    // Not implemented: start() fails with a message saying so.
#else
    int _pid      = -1;
    int _toChild  = -1;  // we write, it reads
    int _fromChild = -1; // it writes, we read
    std::string _buffer;
#endif
};

}  // namespace chess
