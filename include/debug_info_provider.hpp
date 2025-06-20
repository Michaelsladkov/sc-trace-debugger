#pragma once

#define LIBDWARF_STATIC

#include <string>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <libelf.h>
#include <libdwarf/libdwarf.h>

class DwarfException : public std::runtime_error {
public:
    DwarfException(const std::string& message) : runtime_error(message) {}
};

struct SourceLineSpec {
    std::string source_path;
    size_t line;
    size_t column;
    SourceLineSpec(const std::string& file, size_t line_number, size_t column_number) :
        source_path(file),
        line(line_number),
        column(column_number) {}
    bool operator==(const SourceLineSpec& other) const {
        return 
            source_path == other.source_path &&
            line == other.line &&
            column == other.column;
    }
    struct Hasher {
        size_t operator()(const SourceLineSpec& spec) const {
            return 
                std::hash<std::string>{}(spec.source_path) ^
                std::hash<size_t>{}(spec.line) ^
                std::hash<size_t>{}(spec.column);
        }
    };
    std::string to_string() const {
        return source_path + ':' + std::to_string(line) + ':' + std::to_string(column);
    }
};


class NoPcInfoException : public DwarfException {
public:
    NoPcInfoException(const SourceLineSpec& line) : DwarfException("No pc info for line " + line.to_string()) {}
};

class NoSuchLineException : public DwarfException {
public:
    NoSuchLineException(uint64_t pc) : DwarfException("No line corresponds to pc " + std::to_string(pc)) {}
};

enum class LocationType {
    MEMORY,
    REGISTER,
    FRAME_OFFSET
};

struct VariableLocation {
    LocationType type;
    union {
        Dwarf_Addr address;
        Dwarf_Unsigned reg_num;
        Dwarf_Signed offset;
    };
};

struct VariableInfo {
    std::string name;
    std::string type_name;
    VariableLocation location;
    Dwarf_Unsigned size;
};

inline std::ostream& operator<<(std::ostream& stream, const SourceLineSpec& l) {
    stream << l.source_path << ':' << l.line << ':' << l.column;
    return stream;
}

using LineToAddrMap = std::unordered_map<SourceLineSpec, std::vector<uint64_t>, SourceLineSpec::Hasher>;
using AddrToLineMap = std::unordered_map<uint64_t, SourceLineSpec>;
using TypeSizeMap = std::unordered_map<std::string, size_t>;

class DebugInfoProvider {
private:
    std::string elf_file_path;
    int elf_fd = -1;
    Elf* elf_handler = nullptr;
    Dwarf_Debug dbg = nullptr;
    mutable Dwarf_Error err = nullptr;
    LineToAddrMap line_addr_map;
    AddrToLineMap addr_line_map;
    TypeSizeMap type_size_map;
public:
    DebugInfoProvider() = default;
    explicit DebugInfoProvider(const std::string& elf_path, const std::string& common_prefix = "");
    DebugInfoProvider(const DebugInfoProvider& other) = delete;
    DebugInfoProvider(DebugInfoProvider&& other);
    DebugInfoProvider& operator=(const DebugInfoProvider& other) = delete;
    bool empty() const {
        return elf_fd == -1 && (!elf_handler) && (!dbg) && (!err);
    }
    const SourceLineSpec& get_line_by_pc(uint64_t pc) const;
    const std::vector<uint64_t>& get_pc_by_line(const SourceLineSpec& line_spec) const;
    std::vector<VariableInfo> get_available_variables(uint64_t pc) const;
    virtual ~DebugInfoProvider();
};
