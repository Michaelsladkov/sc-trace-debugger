#include "RISCV64_model.hpp"

#include <iostream>
#include <map>
#include <sstream>

#include "RISCV64_decode.hpp"

TraceLine::TraceLine(const std::string& line) {
    std::stringstream view(line);
    view >> time;
    view >> rsv1;
    rsv2 = ' ';
    while (rsv2 == ' ') {
        view >> rsv2;
    }
    view >> std::hex;
    view >> cur_pc;
    view >> instr;
    view >> next_pc;
    if (!view.eof()) {
        std::string reg_update_descr;
        view >> reg_update_descr;
        char type_char = reg_update_descr[0];
        int index = std::stoi(reg_update_descr.c_str() + 1);
        changed_reg = RegisterDescription();
        if (type_char == 'x') {
            changed_reg->type = RegType::INT;
        }
        if (type_char == 'f') {
            changed_reg->type = RegType::FLOAT;
        }
        changed_reg->index = index;
        const char* value_start = reg_update_descr.c_str();
        while (*value_start != '=') {
            ++value_start;
        }
        ++value_start;
        new_reg_val = std::stoull(value_start, nullptr, 16);
    }
}

void RISCV64Model::init(std::istream& trace_input, const std::string& filename) {
    trace_name = filename;
    size_t line_number = 0;
    for (std::string line; trace_input.good(), ++line_number;) {
        std::getline(trace_input, line);
        const char* first_no_space = line.c_str();
        if (line.empty()) {
            break;
        }
        while (*first_no_space == ' ' || *first_no_space == '\t') {
            ++first_no_space;
        }
        if (*first_no_space == '#') {
            continue;
        }
        try {
            TraceLine trace_line(line);
            trace_events.emplace_back(trace_line, *this);
            step_forward();
        } catch (std::exception& e) {
            std::cerr << "Error (" << e.what() << ") on reading line " << line_number << ": " << line << std::endl;
        } catch (...) {
            std::cerr << "Error on reading line " << line_number << ": " << line << std::endl;
        }
    }
    cur_event_id = 0;
    pc = trace_events[cur_event_id].pc;
    for (size_t i = 0; i < 32; ++i) {
        integer_reg_array[i] = 0;
    }
}

void RISCV64Model::set_state_pc(uint64_t address) {
    while(trace_events[cur_event_id].pc != address) {
        step_forward();
        if (cur_event_id >= trace_events.size()) {
            throw NoSuchPcException(address);
        }
    }    
}

bool RISCV64Model::step_forward() {
    if (cur_event_id == trace_events.size()) {
        return false;
    }
    const auto& event = trace_events[cur_event_id];
    pc = event.pc;
    if (event.changed_reg.has_value()) {
        if (event.changed_reg->reg.type == RegType::INT) {
            integer_reg_array[event.changed_reg->reg.index] = event.changed_reg->val;
        }
    }
    ++cur_event_id;
    return true;
}

bool RISCV64Model::step_back() {
    if (cur_event_id == 0) {
        return false;
    }
    --cur_event_id;
    const auto& cur_event = trace_events[cur_event_id];
    if (cur_event.changed_reg.has_value()) {
        uint64_t old_value = cur_event.changed_reg->prev;
        size_t changed_reg_index = cur_event.changed_reg->reg.index;
        integer_reg_array[changed_reg_index] = old_value;
    }
    const auto& prev_event = trace_events[cur_event_id - 1];
    pc = prev_event.pc;
    return true;
}

uint64_t RISCV64Model::read_register(size_t index) const {
    if (index >= 32) {
        std::string reg_name = "x" + std::to_string(index);
        throw NoSuchRegisterException(reg_name);
    }
    return integer_reg_array[index];
}

uint64_t RISCV64Model::read_pc() const {
    return pc;
}

uint64_t RISCV64Model::cur_time() const {
    if (cur_event_id < trace_events.size()) {
        return trace_events.at(cur_event_id).time;
    }
    return trace_events.at(cur_event_id - 1).time;
}

uint64_t RISCV64Model::read_register(const std::string& name) const {
    if (name.starts_with("pc")) {
        return pc;
    }
    if (name[0] != 'x') {
        throw NoSuchRegisterException(name);
    }
    size_t index = std::stoull(name.c_str() + 1);
    return integer_reg_array[index];
}

std::vector<std::pair<std::string, uint64_t>> RISCV64Model::get_all_regs() const {
    std::vector<std::pair<std::string, uint64_t>> res;
    for (size_t i = 0; i < 32; ++i) {
        res.push_back({"x" + std::to_string(i), integer_reg_array[i]});
    }
    return res;
}

uint64_t RISCV64Model::read_memory_dword(uint64_t address) const {
    for (size_t i = cur_event_id; i > 0; --i) {
        const auto& event = trace_events.at(i);
        auto decoded = RISCV64Decode::decode(event.instr);
        if (decoded.type == RISCV64Decode::InstructionType::UNSUPPORTED) {
            continue;
        }
        uint64_t accessed_address;
        uint8_t data_reg_index;
        if (decoded.type == RISCV64Decode::InstructionType::LOAD) {
            auto addr_reg = decoded.content.loadContent.rs1Index;
            data_reg_index = decoded.content.loadContent.rdIndex;
            accessed_address = (integer_reg_array[addr_reg] + decoded.content.loadContent.offset) & ~0x7ULL;
        }
        if (decoded.type == RISCV64Decode::InstructionType::STORE) {
            auto addr_reg = decoded.content.storeContent.rs1Index;
            data_reg_index = decoded.content.storeContent.rs2Index;
            accessed_address = (integer_reg_array[addr_reg] + decoded.content.storeContent.offset) & ~0x7ULL;
        }
        if (accessed_address != address) {
            continue;
        }
        if (decoded.type == RISCV64Decode::InstructionType::LOAD) {
            return event.changed_reg->val;
        }
        for (size_t j = i; j > 0; --j) {
            const auto& event = trace_events.at(j);
            if (!event.changed_reg) {
                continue;
            }
            if (event.changed_reg->reg.index != data_reg_index) {
                continue;
            }
            return event.changed_reg->val;
        }
    }
    return 0;
};

uint32_t RISCV64Model::read_memory_word(uint64_t address) const {
    if (address & 0x3) throw MisalignedAddressException(address, 4);
    const uint64_t base_address = address & ~0x7ULL;
    const uint64_t offset = address - base_address;
    const uint64_t value = read_memory_dword(base_address);
    return (value >> (offset * 8)) & 0xFFFFFFFF;
};

uint16_t RISCV64Model::read_memory_hword(uint64_t address) const {
    if (address & 0x1) throw MisalignedAddressException(address, 2);
    const uint64_t base_address = address & ~0x7ULL;
    const uint64_t offset = address - base_address;
    const uint64_t value = read_memory_dword(base_address);
    return (value >> (offset * 8)) & 0xFFFF;
};

uint8_t RISCV64Model::read_memory_byte(uint64_t address) const {
    const uint64_t base_address = address & ~0x7ULL;
    const uint64_t offset = address - base_address;
    const uint64_t value = read_memory_dword(base_address);
    return (value >> (offset * 8)) & 0xFF;
};
