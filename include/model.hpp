#pragma once

#include <istream>

class IModel {
public:
    virtual void set_state_pc(uint64_t address) = 0;
    virtual void step_forward() = 0;
    virtual void step_back() = 0;
    virtual uint64_t read_register(size_t index) = 0;
    virtual uint64_t read_register(const std::string& name) = 0;
};
