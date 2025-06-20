#include "session.hpp"
#include "executor.hpp"
#include <iostream>
#include <optional>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Not enough arguments\n";
        std::cerr << "Please provide traces root path and elf\n";
        return 1;
    }

    DebugSessionFactory factory;
    std::unique_ptr<Executor> exec;

    try {
        DebugSession session = factory.create_session(argv[1]);
        DebugInfoProvider provider(argv[2], "tests/");
        provider.get_available_variables(0);
        exec = std::make_unique<Executor>(std::move(session), std::move(provider));
    } 
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        return 2;
    }

    std::string input;
    std::cout << '>';
    while (std::getline(std::cin, input)) {
        if (input.empty()) {
            std::cout << '>';
            continue;
        };
        if (input == "exit") {
            break;
        }   
        try {
            exec->execute_command(input);
        } 
        catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
        std::cout << '>';
    }

    return 0;
}
