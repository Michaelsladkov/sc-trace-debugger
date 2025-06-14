#pragma once

#include "session.hpp"
#include "debug_info_provider.hpp"

#include <iostream>
#include <functional>

class UnsupportedCommandException : public std::runtime_error {
public:
    UnsupportedCommandException(const std::string& command) : std::runtime_error("Unsupported command: " + command) {}
};

class Executor {
    DebugSession session;
    DebugInfoProvider debug_info;
    std::ostream* out = &std::cout;
    std::ostream* err = &std::cerr;
public:
    struct CommandParams {
        const std::string& args;
        std::ostream& out;
        std::ostream& err; 
        DebugSession& session;
        DebugInfoProvider& debug_info_provider;
        CommandParams(const std::string& args_, 
                 std::ostream& out_, 
                 std::ostream& err_,
                 DebugSession& session_,
                 DebugInfoProvider& debug_info_provider_)
        : args(args_),
          out(out_),
          err(err_),
          session(session_),
          debug_info_provider(debug_info_provider_) {}
    };
    using CommandObject = std::function<void(CommandParams)>;
    Executor() = default;
    Executor(DebugSession&& debug_session, DebugInfoProvider&& provider) :
        session(std::move(debug_session)),
        debug_info(std::move(provider)) {}
    Executor(const Executor& other) = delete;
    Executor& operator=(const Executor& other) = delete;
    void execute_command(const std::string& command);
};
