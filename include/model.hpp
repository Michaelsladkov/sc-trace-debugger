#pragma once

#include <istream>
#include <stdexcept>
#include <utility>
#include <vector>

class NoSuchPcException : public std::runtime_error {
public:
    NoSuchPcException(uint64_t pc) : runtime_error(std::string("No record for pc=") + std::to_string(pc)) {}
};

class NoSuchRegisterException : public std::runtime_error {
public:
    NoSuchRegisterException(const std::string& reg_name) : runtime_error("Model has no register " + reg_name) {}
};

class IModel {
public:
    virtual void set_state_pc(uint64_t address) = 0;
    virtual bool step_forward() = 0;
    virtual bool step_back() = 0;
    virtual uint64_t read_register(size_t index) const = 0;
    virtual uint64_t read_pc() const = 0;
    virtual uint64_t read_register(const std::string& name) const = 0;
    virtual std::vector<std::pair<std::string, uint64_t>> get_all_regs() const = 0;
    virtual std::string description() const = 0;
    virtual ~IModel() = default;
};

class IModelWithMemory : public IModel {
public:
    virtual uint64_t read_memory_dword(uint64_t address) const = 0;
    virtual uint32_t read_memory_word(uint64_t address) const = 0;
    virtual uint16_t read_memory_hword(uint64_t address) const = 0;
    virtual uint8_t  read_memory_byte(uint64_t address) const = 0;
};
