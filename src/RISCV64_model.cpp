#include "RISCV64_model.hpp"

#include <iostream>
#include <map>
#include <sstream>

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

RISCV64Model::RISCV64Model(std::istream& trace_input) {
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
            trace_events.emplace_back(trace_line);
        } catch (std::exception& e) {
            std::cerr << "Error on reading line " << line_number << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Error on reading line " << line_number << std::endl;
        }
    }
    pc = 0;
    cur_event_id = 0;
    for (size_t i = 0; i < 32; ++i) {
        integer_reg_array[i] = 0;
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
    }
    return true;
}

uint64_t RISCV64Model::read_register(size_t index) const {
    if (index >= sizeof(integer_reg_array)) {
        std::string reg_name = "x" + std::to_string(index);
        throw NoSuchRegisterException(reg_name);
    }
    return integer_reg_array[index];
}

uint64_t RISCV64Model::read_register(const std::string& name) const {
    try {
        return *(name_to_reg_map.at(name));
    } catch (std::out_of_range& e) {
        throw NoSuchRegisterException(name);
    }
}

std::map<std::string, uint64_t> RISCV64Model::get_all_regs() const {
    std::map<std::string, uint64_t> res;
    for (const auto& [name, value_ptr] : name_to_reg_map) {
        res.insert({name, *value_ptr});
    }
    return res;
}
