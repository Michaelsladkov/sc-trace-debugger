#pragma once

#include "model.hpp"
#include "session_memory.hpp"

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


class MisalignedAddressException : public std::runtime_error {
public:
    MisalignedAddressException(uint64_t address, uint16_t size) :
        runtime_error(
            std::string("Address ") +
            std::to_string(address) +
            " not aligned for size " +
            std::to_string(size)) {}
};

class DebugSession {
    std::vector<std::shared_ptr<IModel>> cpu_array;
    std::set<uint64_t> breakpoint_addresses;
    Memory memory;
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
            bool cpu_alive = false;
            for (size_t i = 0; i < cpu_array.size(); ++i) {
                auto& cpu = cpu_array[i];
                cpu_alive |= cpu->step_forward();
                bool reached_break_point = breakpoint_addresses.contains(cpu->read_pc());
                need_stop |= reached_break_point;
                if (reached_break_point) {
                    ret = i;
                    break;
                }
            }
            if (!cpu_alive) {
                break;
            }
        }
        return ret;
    }
    const auto& get_harts() {
        return cpu_array;
    }
    uint64_t read_memory_dword(uint64_t address) const {
        if (address & 0x7) throw MisalignedAddressException(address, 8);
        return memory.getValue(address, cpu_array.at(active_hart)->cur_time());
    };

    uint32_t read_memory_word(uint64_t address) const {
        if (address & 0x3) throw MisalignedAddressException(address, 4);
        const uint64_t base_address = address & ~0x7ULL;
        const uint64_t offset = address - base_address;
        const uint64_t value = read_memory_dword(base_address);
        return (value >> (offset * 8)) & 0xFFFFFFFF;
    };

    uint16_t read_memory_hword(uint64_t address) const {
        if (address & 0x1) throw MisalignedAddressException(address, 2);
        const uint64_t base_address = address & ~0x7ULL;
        const uint64_t offset = address - base_address;
        const uint64_t value = read_memory_dword(base_address);
        return (value >> (offset * 8)) & 0xFFFF;
    };

    uint8_t read_memory_byte(uint64_t address) const {
        const uint64_t base_address = address & ~0x7ULL;
        const uint64_t offset = address - base_address;
        const uint64_t value = read_memory_dword(base_address);
        return (value >> (offset * 8)) & 0xFF;
    };
    friend class DebugSessionFactory;
};

class DebugSessionFactory {
public:
    DebugSession create_session(const std::string& trace_dir_path);
};
