#include "debug_info_provider.hpp"

#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include <iostream>

namespace {
    std::pair<LineToAddrMap, AddrToLineMap> build_maps(Dwarf_Debug dbg, Dwarf_Error err, const std::string common_prefix) {
        LineToAddrMap line_addr_ret;
        AddrToLineMap addr_line_ret;
        Dwarf_Bool      is_info;
        Dwarf_Unsigned  cu_header_length;
        Dwarf_Half      version_stamp;
        Dwarf_Off       abbrev_offset;
        Dwarf_Half      address_size;
        Dwarf_Half      length_size;
        Dwarf_Half      extension_size;
        Dwarf_Sig8      type_signature;
        Dwarf_Unsigned  typeoffset;
        Dwarf_Unsigned  next_cu_header_offset;
        Dwarf_Half      header_cu_type;
        size_t cu_cnt = 0;
        while (dwarf_next_cu_header_d(dbg, is_info, &cu_header_length, 
                                  &version_stamp, &abbrev_offset, 
                                  &address_size, &length_size,
                                  &extension_size, &type_signature,
                                  &typeoffset, &next_cu_header_offset,
                                  &header_cu_type, &err) == DW_DLV_OK) {
        
            Dwarf_Die cu_die;
            if (dwarf_siblingof_b(dbg, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
                continue;
            }
            Dwarf_Unsigned     version;
            Dwarf_Small        table_count;
            Dwarf_Line_Context linecontext;
            if(dwarf_srclines_b(cu_die, &version, &table_count, &linecontext, &err) != DW_DLV_OK) {
                throw DwarfException("failed to get linecontext");
            }
            Dwarf_Line* linebuf;
            Dwarf_Signed linecount;
            if (dwarf_srclines_from_linecontext(linecontext, &linebuf, &linecount, &err) != DW_DLV_OK) {
                dwarf_srclines_dealloc_b(linecontext);
                throw DwarfException("failed to read lines");
            }
            for (size_t i = 0; i < linecount; ++i) {
                Dwarf_Line l = linebuf[i];
                Dwarf_Addr addr;
                char* raw_src_name;
                Dwarf_Unsigned line_num;
                Dwarf_Unsigned column;
                if (
                    dwarf_linesrc(l, &raw_src_name, &err) != DW_DLV_OK ||
                    dwarf_lineno(l, &line_num, &err) != DW_DLV_OK ||
                    dwarf_lineoff_b(l, &column, &err) != DW_DLV_OK ||
                    dwarf_lineaddr(l, &addr, &err) != DW_DLV_OK
                ) {
                    dwarf_dealloc(dbg, raw_src_name, DW_DLA_STRING);
                    dwarf_srclines_dealloc_b(linecontext);
                    throw DwarfException("failed to read line info");
                }
                std::string src_name(raw_src_name);
                if (!common_prefix.empty()) {
                    size_t prefix_begin = src_name.rfind(common_prefix);
                    src_name = src_name.substr(prefix_begin);
                }
                SourceLineSpec spec(src_name, line_num, column);
                // std::cerr << spec << "->" << std::hex << addr << std::dec << std::endl;
                addr_line_ret.emplace(addr, spec);
                spec.column = 0;
                line_addr_ret[spec].emplace_back(addr);
                dwarf_dealloc(dbg, raw_src_name, DW_DLA_STRING);
            }
            ++cu_cnt;
            dwarf_srclines_dealloc_b(linecontext);
        }
        std::cerr << "dwarf cu processed: " << cu_cnt << std::endl;
        return std::make_pair(line_addr_ret, addr_line_ret);
    }
}

DebugInfoProvider::DebugInfoProvider(const std::string& elf_path, const std::string& common_prefix) : elf_file_path(elf_path) {
    if (elf_version(EV_CURRENT) == EV_NONE)
        throw DwarfException("ELF library too old");
    elf_fd = open(elf_path.c_str(), O_RDONLY);
    if (elf_fd < 0) {
        throw DwarfException("failed to open elf");
    }
    elf_handler = elf_begin(elf_fd, ELF_C_READ, nullptr);

    if (elf_handler == nullptr) {
        perror("Elf error");
        close(elf_fd);
        throw DwarfException("failed to open elf stream");
    }

    int res = dwarf_elf_init(elf_handler, DW_DLC_READ, nullptr, nullptr, &dbg, &err);
    std::cerr << res << std::endl;
    
    if (res != DW_DLV_OK) {
        elf_end(elf_handler);
        close(elf_fd);
        throw DwarfException("failed to read dwarf");
    }
    auto [l2a, a2l] = build_maps(dbg, err, common_prefix);
    line_addr_map = std::move(l2a);
    addr_line_map = std::move(a2l);
}

DebugInfoProvider::DebugInfoProvider(DebugInfoProvider&& other) :
    elf_file_path(other.elf_file_path),
    elf_fd(other.elf_fd),
    elf_handler(other.elf_handler),
    dbg(other.dbg),
    err(other.err),
    line_addr_map(std::move(other.line_addr_map)),
    addr_line_map(std::move(other.addr_line_map)) {
        other.elf_fd = -1;
        other.elf_handler = nullptr;
        other.dbg = nullptr;
        other.err = nullptr;
    }


const SourceLineSpec& DebugInfoProvider::get_line_by_pc(uint64_t pc) const {
    auto res = addr_line_map.find(pc);
    if (res == addr_line_map.end()) {
        throw NoSuchLineException(pc);
    }
    return res->second;
}

const std::vector<uint64_t>& DebugInfoProvider::get_pc_by_line(const SourceLineSpec& line_spec) const {
    auto res = line_addr_map.find(line_spec);
    if (res == line_addr_map.end()) {
        throw NoPcInfoException(line_spec);
    }
    return res->second;
}

DebugInfoProvider::~DebugInfoProvider() {
    if (!empty()) {
        dwarf_finish(dbg, &err);
        elf_end(elf_handler);
        close(elf_fd);
    }
}

