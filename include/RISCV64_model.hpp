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
    const std::unordered_map<std::string, uint64_t*> name_to_reg_map = {
        {"x0", integer_reg_array},
        {"x1", integer_reg_array + 1},
        {"x2", integer_reg_array + 2},
        {"x3", integer_reg_array + 3},
        {"x4", integer_reg_array + 4},
        {"x5", integer_reg_array + 5},
        {"x6", integer_reg_array + 6},
        {"x7", integer_reg_array + 7},
        {"x8", integer_reg_array + 8},
        {"x9", integer_reg_array + 9},
        {"x10", integer_reg_array + 10},
        {"x11", integer_reg_array + 11},
        {"x12", integer_reg_array + 12},
        {"x13", integer_reg_array + 13},
        {"x14", integer_reg_array + 14},
        {"x15", integer_reg_array + 15},
        {"x16", integer_reg_array + 16},
        {"x17", integer_reg_array + 17},
        {"x18", integer_reg_array + 18},
        {"x19", integer_reg_array + 19},
        {"x20", integer_reg_array + 20},
        {"x21", integer_reg_array + 21},
        {"x22", integer_reg_array + 22},
        {"x23", integer_reg_array + 23},
        {"x24", integer_reg_array + 24},
        {"x25", integer_reg_array + 25},
        {"x26", integer_reg_array + 26},
        {"x27", integer_reg_array + 27},
        {"x28", integer_reg_array + 28},
        {"x29", integer_reg_array + 29},
        {"x30", integer_reg_array + 30},
        {"x31", integer_reg_array + 31},
        {"pc", &pc}
    };
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
