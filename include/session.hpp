#pragma once

#include "model.hpp"

#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

class NoSuchHartException : public std::runtime_error {
public:
    NoSuchHartException(size_t hart_id) : std::runtime_error(std::string("No hart with id ") + std::to_string(hart_id)) {}
};

class SessionCreationError : public std::runtime_error {
public:
    SessionCreationError() : std::runtime_error("Failed to create session check trace directory") {}
};

class DebugSession {
    std::vector<std::unique_ptr<IModel>> cpu_array;
    std::set<uint64_t> breakpoint_addresses;
    size_t active_hart = 0;
public:
    DebugSession(const DebugSession& other) = delete;
    DebugSession(DebugSession&& other) = default;
    DebugSession() = default;
    DebugSession& operator=(const DebugSession& other) = delete;
    DebugSession& operator=(DebugSession&& other) = default;
    const IModel* operator->() const {
        return cpu_array[active_hart].get();
    }
    IModel* operator->() {
        return cpu_array[active_hart].get();
    }
    size_t get_active_hart() const {
        return active_hart;
    }
    void set_active_hart(size_t hart_id) {
        if (hart_id >= cpu_array.size()) {
            throw NoSuchHartException(hart_id);
        }
        active_hart = hart_id;
    }
    void add_break_point(uint64_t addr) noexcept {
        breakpoint_addresses.insert(addr);
    }
    void remove_break_point(uint64_t addr) noexcept {
        breakpoint_addresses.erase(addr);
    }
    const auto& get_harts() {
        return cpu_array;
    }
    friend class DebugSessionFactory;
};

class DebugSessionFactory {
public:
    DebugSession create_session(const std::string& trace_dir_path);
};
