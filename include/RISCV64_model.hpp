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

struct RegisterUpdateEvent {
    RegisterDescription reg;
    uint64_t val;
    uint64_t prev;
    RegisterUpdateEvent(const RegisterDescription& descr, uint64_t new_val, uint64_t prev_val = 0) :
        reg(descr),
        val(new_val),
        prev(prev_val) {}
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
    uint64_t time;
    uint64_t pc;
    uint32_t instr;
    std::optional<RegisterUpdateEvent> changed_reg = std::nullopt;
    TraceEntry(const TraceLine& line, const IModel& model) :
        time(line.time), 
        pc(line.cur_pc),
        instr(line.instr) {
            if (line.changed_reg) {
                changed_reg = RegisterUpdateEvent(
                    line.changed_reg.value(),
                    line.new_reg_val.value(),
                    model.read_register(line.changed_reg.value().index)
                    );                                             
            }
        }
};

struct MemoryEntry {
    uint64_t value = 0;
    uint64_t modification_time = 0;
    MemoryEntry(uint64_t val, uint64_t time) : value(val), modification_time(time) {}
};

using Memory = std::unordered_map<uint64_t, std::pair<MemoryEntry, MemoryEntry>>;

class RISCV64Model : public IModel {
protected:
    virtual void init(std::istream& trace_input, const std::string& filename);
    std::vector<TraceEntry> trace_events;
    size_t cur_event_id = 0;
    uint64_t integer_reg_array[32] = {0};
    uint64_t pc = 0;
    size_t hart_id = 0;
    std::string trace_name;
public:
    virtual void set_state_pc(uint64_t address) override;
    virtual bool step_forward() override;
    virtual bool step_back() override;
    virtual uint64_t read_register(size_t index) const override;
    virtual uint64_t read_pc() const override;
    virtual uint64_t read_register(const std::string& name) const override;
    virtual std::vector<std::pair<std::string, uint64_t>> get_all_regs() const override;
    virtual std::string description() const override {
        return "Basic RV64 model (" + trace_name + ')';
    }
    friend class DebugSessionFactory;
};
