#pragma once

#include "model.hpp"

#include <vector>

class RISCV64Model : public IModel {
protected:
    enum class RegType {
        INT,
        FLOAT
    };
    struct RegisterDescription {
        size_t index;
        RegType type;
    };
    
    struct TraceEntry {
        uint64_t pc;
        RegisterDescription reg;
        uint64_t reg_val;
    };
    std::vector<TraceEntry> trace_events;
    size_t cur_event_id;
    uint64_t integer_reg_array[32] = {0};
    uint64_t float_reg_array[32] = {0};
    uint64_t pc;
public:
    RISCV64Model(std::istream& trace_input);
    virtual void set_state_pc(uint64_t address) override;
    virtual void step_forward() override;
    virtual void step_back() override;
    virtual uint64_t read_register(size_t index) override;
    virtual uint64_t read_register(const std::string& name) override;
};
