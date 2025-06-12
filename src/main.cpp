#include "session.hpp"
#include "executor.hpp"
#include <iostream>
#include <optional>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "No trace directory provided\n";
        return 1;
    }

    DebugSessionFactory factory;
    Executor exec;

    try {
        DebugSession session = factory.create_session(argv[1]);
        exec = Executor(std::move(session));
    } 
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        return 2;
    }

    std::string input;
    std::cout << '>';
    while (std::getline(std::cin, input)) {
        if (input.empty()) continue;        
        try {
            exec.execute_command(input);
        } 
        catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }
        std::cout << '>';
    }

    return 0;
}