#include "executor.hpp"
#include "model.hpp"

#include <unordered_map>

namespace {
    uint64_t parse_value_maybe_hex(const std::string& arg) {
        if (arg.starts_with("0x")) {
            return std::stoull(arg.c_str() + 2, nullptr, 16);
        } else {
            return std::stoull(arg.c_str());
        }
    }

    void reg_command(Executor::CommandParams p) {
        if (p.args.size() == 0) {
            auto res = p.session->get_all_regs();
            for(auto& [name, value] : res) {
                p.out << name << "=" << std::hex << "0x" << value << std::dec << std::endl;
            }
            return;
        }
        auto res = p.session->read_register(p.args);
        p.out << std::hex << "0x" << res << std::dec << std::endl; 
    }

    void hart_command(Executor::CommandParams p) {
        if (p.args.size() == 0) {
            const auto& res = p.session.get_harts();
            for (size_t i = 0; i < res.size(); ++i) {
                p.out << i << ": " << res[i]->description() <<
                    (i == p.session.get_active_hart() ? "*" : "") << std::endl;
            }
            return;
        }
        size_t hart_id = std::stoull(p.args);
        p.session.set_active_hart(hart_id);
        p.out << hart_id << ": " << p.session->description() << std::endl;
    }

    void step_command(Executor::CommandParams p) {
        p.session->step_forward();
    }

    void step_back_command(Executor::CommandParams p) {
        p.session->step_back();
    }

    void run_till_command(Executor::CommandParams p) {
        uint64_t target_pc = parse_value_maybe_hex(p.args);
        p.session->set_state_pc(target_pc);
    }

    void add_break_point_command(Executor::CommandParams p) {
        uint64_t target_pc;
        try {
            if (p.args[0] >= '0' && p.args[0] <= '9') {
                target_pc = parse_value_maybe_hex(p.args);
            } else {
                size_t colon_pos = p.args.find(':');
                if (colon_pos == std::string::npos) {
                    p.err << "unsupported breakpoint target\n";
                    return;
                } else {
                    std::string filename = p.args.substr(0, colon_pos);
                    size_t line_num = std::stoull(p.args.c_str() + colon_pos + 1);
                    SourceLineSpec spec(filename, line_num, 0);
                    target_pc = p.debug_info_provider.get_pc_by_line(spec)[0];
                }
            }
        } catch(std::runtime_error& e) {
            p.err << "error occured: " << e.what() << std::endl;
        }
        p.session.add_break_point(target_pc);
        p.out << "break point set for " << std::hex << "0x" << target_pc << std::dec << std::endl;
    }

    void remove_break_point_command(Executor::CommandParams p) {
        uint64_t target_pc;
        try {
            if (p.args[0] >= '0' && p.args[0] <= '9') {
                target_pc = parse_value_maybe_hex(p.args);
            } else {
                size_t colon_pos = p.args.find(':');
                if (colon_pos == std::string::npos) {
                    p.err << "unsupported breakpoint target\n";
                    return;
                } else {
                    std::string filename = p.args.substr(0, colon_pos);
                    size_t line_num = std::stoull(p.args.c_str() + colon_pos + 1);
                    SourceLineSpec spec(filename, line_num, 0);
                    target_pc = p.debug_info_provider.get_pc_by_line(spec)[0];
                }
            }
        } catch(std::runtime_error& e) {
            p.err << "error occured: " << e.what() << std::endl;
        }
        if (p.session.remove_break_point(target_pc)) {
            p.out << "break point set for " << std::hex << "0x" << target_pc << std::dec << std::endl;
        } else {
            p.out << "no break point found\n";
        }
    }

    void resume_command(Executor::CommandParams p) {
        std::optional<size_t> ret;
        if (p.args.size() == 0) {
            ret = p.session.run_all();
        } else {
            size_t hart_id = std::stoull(p.args);
            p.session.set_active_hart(hart_id);
            ret = p.session.run();
        }
        if (ret) {
            p.out << "hart " << ret.value() << " reached break point\n";
        } else {
            p.out << "all runned harts reached end of trace\n";
        }
    }

    void line_command(Executor::CommandParams p) {
        uint64_t pc;
        if (p.args.size() == 0) {
            pc = p.session->read_pc(); 
        } else {
            pc = parse_value_maybe_hex(p.args);
        }
        try {
            auto res = p.debug_info_provider.get_line_by_pc(pc);
            p.out << res << std::endl;
        } catch(NoSuchLineException& e) {
            p.out << e.what() << std::endl;
        }
    }

    void variables_command(Executor::CommandParams p) {
        const uint64_t pc = p.session->read_pc();
        auto variables = p.debug_info_provider.get_available_variables(pc);
        const uint64_t sp = p.session->read_register("x2");
        for (auto& v : variables) {
            uint64_t addr;
            if (v.location.type == LocationType::MEMORY) {
                addr = v.location.value;
            } else if (v.location.type == LocationType::FRAME_OFFSET) {
                addr = sp + v.location.value;
            } else {
                addr = -1;
            }
            p.out << v.name << ": " << v.type_name 
                << " (" << std::hex
                << addr
                << ")" << std::dec << std::endl;
        }
    }

    std::unordered_map<std::string, Executor::CommandObject> commands = {
        {"reg", reg_command},
        {"hart", hart_command},
        {"step", step_command},
        {"s", step_command},
        {"step_back", step_back_command},
        {"sb", step_back_command},
        {"run-till", run_till_command},
        {"rt", run_till_command},
        {"bp", add_break_point_command},
        {"rbp", remove_break_point_command},
        {"resume", resume_command},
        {"run", resume_command},
        {"line", line_command},
        {"l", line_command},
        {"variables", variables_command}
    };
}

void Executor::execute_command(const std::string& command) {
    std::string command_type;
    size_t space_pos = command.find(' ');
    if (space_pos == std::string::npos) {
        command_type = command;
    } else {
        command_type = command.substr(0, space_pos);
    }
    size_t args_pos = space_pos;
    if (args_pos != std::string::npos) while (command[args_pos] == ' ' || command[args_pos] == '\t') {
        ++args_pos;
    }
    std::string command_args = args_pos == std::string::npos ? "" : command.substr(args_pos);
    if (!commands.contains(command_type)) {
        throw UnsupportedCommandException(command_type);
    }
    commands[command_type](Executor::CommandParams(command_args, *out, *err, session, debug_info));
}
