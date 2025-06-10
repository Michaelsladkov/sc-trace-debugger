#include "RISCV64_model.hpp"

#include <sstream>

#include <iostream>

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
        new_reg_val = std::stoull(value_start);
    }
}

RISCV64Model::RISCV64Model(std::istream& trace_input) {
    std::string first_line;
    std::getline(trace_input, first_line);
    size_t id_start_pos = first_line.rfind(' ');
    hart_id = std::stoull(first_line.c_str() + id_start_pos);
    for (std::string line; std::getline(trace_input, line);) {
        const char* first_no_space = line.c_str();
        while (*first_no_space == ' ' || *first_no_space == '\t') {
            ++first_no_space;
        }
        if (*first_no_space == '#') {
            continue;
        }
        TraceLine trace_line(line);
        trace_events.emplace_back(trace_line);
    }
}

void RISCV64Model::set_state_pc(uint64_t address) {
    cur_event_id = 0;
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
    ++cur_event_id;
    const auto& event = trace_events[cur_event_id];
    pc = event.pc;
    if (event.changed_reg.has_value()) {
        if (event.changed_reg->type == RegType::INT) {
            integer_reg_array[event.changed_reg->index] = event.new_reg_val.value();
            return true;
        }
        if (event.changed_reg->type == RegType::FLOAT) {
            float_reg_array[event.changed_reg->index] = event.new_reg_val.value();
            return true;
        }
    }
    return true;
}

bool RISCV64Model::step_back() {
    if (cur_event_id == 0) {
        return false;
    }
    --cur_event_id;
    const auto& event = trace_events[cur_event_id];
    pc = event.pc;
    if (event.changed_reg.has_value()) {
        size_t i = cur_event_id - 1;
        while (trace_events[i].changed_reg != event.changed_reg && i > 0) {
            --i;
        }
        uint64_t old_value = i == 0 ? 0 : trace_events[i].new_reg_val.value();
        if (event.changed_reg->type == RegType::INT) {
            integer_reg_array[event.changed_reg->index] = old_value;
            return true;
        }
        if (event.changed_reg->type == RegType::FLOAT) {
            float_reg_array[event.changed_reg->index] = old_value;
            return true;
        }
    }
    return true;
}

uint64_t RISCV64Model::read_register(size_t index) {
    return integer_reg_array[index];
}

uint64_t RISCV64Model::read_register(const std::string& name) {
    return *(name_to_reg_map.at(name));
}
