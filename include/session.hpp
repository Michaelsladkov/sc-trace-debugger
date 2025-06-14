#pragma once

#include "model.hpp"

#include <memory>
#include <optional>
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
    std::vector<std::shared_ptr<IModel>> cpu_array;
    std::set<uint64_t> breakpoint_addresses;
    size_t active_hart = 0;
public:
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
    bool remove_break_point(uint64_t addr) noexcept {
        return breakpoint_addresses.erase(addr) > 0;
    }
    std::optional<size_t> run() {
        while (!breakpoint_addresses.contains(cpu_array[active_hart]->read_pc())) {
            if (!cpu_array[active_hart]->step_forward()) {
                return active_hart;
                break;
            }
        }
        return std::nullopt;
    }
    std::optional<size_t> run_all() {
        bool need_stop = false;
        std::optional<size_t> ret = std::nullopt;
        while (!need_stop) {
            for (size_t i = 0; i < cpu_array.size(); ++i) {
                auto& cpu = cpu_array[i];
                cpu->step_forward();
                bool reached_break_point = breakpoint_addresses.contains(cpu->read_pc());
                need_stop |= reached_break_point;
                if (reached_break_point) {
                    ret = i;
                    break;
                }
            }
        }
        return ret;
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
