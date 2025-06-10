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
    uint64_t float_reg_array[32] = {0};
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
        {"f0", float_reg_array},
        {"f1", float_reg_array + 1},
        {"f2", float_reg_array + 2},
        {"f3", float_reg_array + 3},
        {"f4", float_reg_array + 4},
        {"f5", float_reg_array + 5},
        {"f6", float_reg_array + 6},
        {"f7", float_reg_array + 7},
        {"f8", float_reg_array + 8},
        {"f9", float_reg_array + 9},
        {"f10", float_reg_array + 10},
        {"f11", float_reg_array + 11},
        {"f12", float_reg_array + 12},
        {"f13", float_reg_array + 13},
        {"f14", float_reg_array + 14},
        {"f15", float_reg_array + 15},
        {"f16", float_reg_array + 16},
        {"f17", float_reg_array + 17},
        {"f18", float_reg_array + 18},
        {"f19", float_reg_array + 19},
        {"f20", float_reg_array + 20},
        {"f21", float_reg_array + 21},
        {"f22", float_reg_array + 22},
        {"f23", float_reg_array + 23},
        {"f24", float_reg_array + 24},
        {"f25", float_reg_array + 25},
        {"f26", float_reg_array + 26},
        {"f27", float_reg_array + 27},
        {"f28", float_reg_array + 28},
        {"f29", float_reg_array + 29},
        {"f30", float_reg_array + 30},
        {"f31", float_reg_array + 31}
    };
public:
    RISCV64Model(std::istream& trace_input);
    virtual void set_state_pc(uint64_t address) override;
    virtual bool step_forward() override;
    virtual bool step_back() override;
    virtual uint64_t read_register(size_t index) override;
    virtual uint64_t read_register(const std::string& name) override;
};
