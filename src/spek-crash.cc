// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "spek-config.h"

#if defined(OS_UNIX) && SPEK_STATIC_LINK && defined(__GNUC__)

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

namespace
{

volatile sig_atomic_t handling_signal = 0;

void write_message(const char *message, size_t length)
{
    const char *current = message;
    while (length != 0) {
        const ssize_t written = write(STDERR_FILENO, current, length);
        if (written <= 0) {
            return;
        }
        current += written;
        length -= static_cast<size_t>(written);
    }
}

void report_fatal_signal(int signal_number)
{
    // Do not recursively enter the reporter if unwinding itself fails.
    if (handling_signal != 0) {
        _exit(128 + signal_number);
    }
    handling_signal = 1;

    static constexpr char prefix[] = "\nSpek: fatal signal (";
    write_message(prefix, sizeof(prefix) - 1);
    static constexpr char unknown[] = "unknown";
    static constexpr char sigfpe[] = "SIGFPE";
    static constexpr char sigsegv[] = "SIGSEGV";
    static constexpr char sigbus[] = "SIGBUS";
    const char *name = unknown;
    size_t name_length = sizeof(unknown) - 1;
    if (signal_number == SIGFPE) {
        name = sigfpe;
        name_length = sizeof(sigfpe) - 1;
    } else if (signal_number == SIGSEGV) {
        name = sigsegv;
        name_length = sizeof(sigsegv) - 1;
    } else if (signal_number == SIGBUS) {
        name = sigbus;
        name_length = sizeof(sigbus) - 1;
    }
    write_message(name, name_length);
    static constexpr char middle[] = ") during startup or execution; backtrace follows.\n";
    write_message(middle, sizeof(middle) - 1);

    void *frames[64];
    const int count = backtrace(frames, static_cast<int>(sizeof(frames) / sizeof(frames[0])));
    if (count > 0) {
        backtrace_symbols_fd(frames, count, STDERR_FILENO);
    }

    static constexpr char suffix[] = "Spek: end of backtrace.\n";
    write_message(suffix, sizeof(suffix) - 1);
    _exit(128 + signal_number);
}

// Run before ordinary C++ static constructors. This is intentional: the
// reported Linux failures happen before main(), where a normal wx or main()
// signal handler cannot see them.
__attribute__((constructor(101))) void install_fatal_signal_handlers()
{
    struct sigaction action = {};
    action.sa_handler = &report_fatal_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_NODEFER;
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
}

} // namespace

#endif
