#pragma once

#include "model.hpp"

#include <optional>
#include <unordered_map>
#include <vector>

enum class RegType {
    INT,
    FLOAT
};
struct RegisterDescription {
    size_t index;
    RegType type;
    bool operator==(const RegisterDescription& other) const {
        return index == other.index && type == other.type;
    }
    bool operator!=(const RegisterDescription& other) const {
        return !((*this) == other);
    }
};

struct TraceLine {
    uint64_t time;
    int rsv1;
    char rsv2;
    uint64_t cur_pc;
    uint32_t instr;
    uint64_t next_pc;
    std::optional<RegisterDescription> changed_reg = std::nullopt;
    std::optional<uint64_t> new_reg_val = std::nullopt;
    TraceLine() = default;
    explicit TraceLine(const std::string& line);
};

struct TraceEntry {
    uint64_t pc;
    std::optional<RegisterDescription> changed_reg = std::nullopt;
    std::optional<uint64_t> new_reg_val = std::nullopt;
    TraceEntry(const TraceLine& line) : 
        pc(line.cur_pc),
        changed_reg(line.changed_reg),
        new_reg_val(line.new_reg_val) {}
};

class RISCV64Model : public IModel {
protected:
    std::vector<TraceEntry> trace_events;
    size_t cur_event_id = 0;
    uint64_t integer_reg_array[32] = {0};
    uint64_t pc = 0;
    size_t hart_id = 0;
public:
    RISCV64Model(std::istream& trace_input);
    virtual void set_state_pc(uint64_t address) override;
    virtual bool step_forward() override;
    virtual bool step_back() override;
    virtual uint64_t read_register(size_t index) const override;
    virtual uint64_t read_register(const std::string& name) const override;
    virtual std::map<std::string, uint64_t> get_all_regs() const override;
    virtual std::string description() const override {
        return "Basic RV64 model";
    }
};
