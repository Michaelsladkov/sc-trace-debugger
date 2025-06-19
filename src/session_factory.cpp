#include "session.hpp"

#include "RISCV64_model.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <iostream>

namespace {
    std::vector<std::string> get_trace_log_files(const std::string& dirPath) {
        std::vector<std::string> result;
        namespace fs = std::filesystem;
        fs::path directory(dirPath);
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            throw SessionCreationError();
        }
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;
            std::string filename = entry.path().filename().string();
            if (filename.find("trace_log") != std::string::npos &&
                filename.find("csr") == std::string::npos) {
                result.push_back(entry.path().string());
            }
        }
        return result;
    }
}

DebugSession DebugSessionFactory::create_session(const std::string& trace_dir_path) {
    auto traces = get_trace_log_files(trace_dir_path);
    std::sort(traces.begin(), traces.end());
    DebugSession res;
    for (const auto& trace : traces) {
        std::cerr << "processing " << trace << " of total " << traces.size() << " traces\n";
        std::ifstream trace_stream(trace);
        std::string trace_name = trace.substr(trace.rfind('/') + 1);
        auto cpu = std::make_unique<RISCV64Model>();
        cpu->init(trace_stream, trace_name);
        res.cpu_array.emplace_back(std::move(cpu));
    }
    return res;
}
