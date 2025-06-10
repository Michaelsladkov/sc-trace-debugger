#pragma once

#include <istream>
#include <stdexcept>

class NoSuchPcException : public std::runtime_error {
public:
    NoSuchPcException(uint64_t pc) : runtime_error("No record for pc=" + pc) {}
};

class IModel {
public:
    virtual void set_state_pc(uint64_t address) = 0;
    virtual bool step_forward() = 0;
    virtual bool step_back() = 0;
    virtual uint64_t read_register(size_t index) = 0;
    virtual uint64_t read_register(const std::string& name) = 0;
};
