#pragma once

#include "session.hpp"

#include <iostream>
#include <functional>

class UnsupportedCommandException : std::runtime_error {
public:
    UnsupportedCommandException(const std::string& command) : std::runtime_error("Unsupported command: " + command) {}
};

class Executor {
    DebugSession session;
    std::ostream& out = std::cout;
    std::ostream& err = std::cerr;
public:
    struct CommandParams {
        const std::string& args;
        std::ostream& out;
        std::ostream& err; 
        DebugSession& session;
        CommandParams(const std::string& args_, 
                 std::ostream& out_, 
                 std::ostream& err_,
                 DebugSession& session_)
        : args(args_),
          out(out_),
          err(err_),
          session(session_) {}
    };
    using CommandObject = std::function<void(CommandParams)>;
    Executor(DebugSession&& debug_session) : session(std::move(debug_session)) {}
    void execute_command(const std::string& command);
};
