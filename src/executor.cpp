#include "executor.hpp"
#include "model.hpp"

#include <unordered_map>

namespace {
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

    std::unordered_map<std::string, Executor::CommandObject> commands = {
        {"reg", reg_command},
        {"hart", hart_command},
        {"step", step_command},
        {"s", step_command}
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
    commands[command_type](Executor::CommandParams(command_args, *out, *err, session));
}
