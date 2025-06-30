#include "debug_info_provider.hpp"

#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <libdwarf/dwarf.h>
#include <sstream>
#include <cstring>

#include <iostream>

namespace {
    std::pair<LineToAddrMap, AddrToLineMap> build_maps(Dwarf_Debug dbg, Dwarf_Error err, const std::string common_prefix) {
        LineToAddrMap line_addr_ret;
        AddrToLineMap addr_line_ret;
        Dwarf_Bool      is_info = true;
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
                addr_line_ret.emplace(addr, spec);
                spec.column = 0;
                line_addr_ret[spec].emplace_back(addr);
                dwarf_dealloc(dbg, raw_src_name, DW_DLA_STRING);
            }
            ++cu_cnt;
            dwarf_srclines_dealloc_b(linecontext);
        }
        return std::make_pair(line_addr_ret, addr_line_ret);
    }

    struct InternalTypeInfo {
        std::string name;
        size_t size;
    };

    std::string get_type_name(Dwarf_Die type_die, Dwarf_Debug dbg, Dwarf_Error err) {
        char* type_name;
        std::string ret;
        if (dwarf_diename(type_die, &type_name, &err) == DW_DLV_OK) {
            ret = type_name;
            dwarf_dealloc(dbg, type_name, DW_DLA_STRING);
        } else {
            throw DwarfException("failed to read type name");
        }
        return ret;
    }

    ssize_t get_type_size(Dwarf_Die type_die, Dwarf_Debug dbg, Dwarf_Error err) {
        Dwarf_Attribute size_attr;
        ssize_t ret;
        if (dwarf_attr(type_die, DW_AT_byte_size, &size_attr, &err) == DW_DLV_OK) {
            Dwarf_Unsigned size;
            if (dwarf_formudata(size_attr, &size, &err) == DW_DLV_OK) {
                ret = size;
            } else {
                dwarf_dealloc(dbg, size_attr, DW_DLA_ATTR);
                throw DwarfException("failed to read size value");
            }
            dwarf_dealloc(dbg, size_attr, DW_DLA_ATTR);
        } else {
            throw DwarfException("failed to read size attribute");
        }
        return ret;
    }

    ssize_t get_typedef_size_recursive(Dwarf_Die type_die, Dwarf_Debug dbg, Dwarf_Error err) {
        size_t res = -1;
        Dwarf_Attribute type_attr = nullptr;
        if (dwarf_attr(type_die, DW_AT_type, &type_attr, &err) == DW_DLV_OK) {
            Dwarf_Off type_ref;
            if (dwarf_global_formref(type_attr, &type_ref, &err) == DW_DLV_OK) {
                Dwarf_Die hidden_type_die;
                if (dwarf_offdie_b(dbg, type_ref, true, &hidden_type_die, &err) != DW_DLV_OK) {
                    dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
                    throw DwarfException("failed to read hidden type DIE");
                }
                Dwarf_Half hidden_type_tag;
                dwarf_tag(hidden_type_die, &hidden_type_tag, &err);
                if (hidden_type_tag == DW_TAG_typedef) {
                    res = get_typedef_size_recursive(hidden_type_die, dbg, err);
                } else {
                    res = get_type_size(hidden_type_die, dbg, err);
                }
            }
        } else {
            throw DwarfException("failed to read type attribute");
        }
        dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
        return res;
    }

    InternalTypeInfo get_type_info(Dwarf_Die type_die, Dwarf_Debug dbg, Dwarf_Error err) {
        Dwarf_Half type_tag;
        dwarf_tag(type_die, &type_tag, &err);
        InternalTypeInfo ret;
        if (type_tag == DW_TAG_base_type) {
            ret.name = get_type_name(type_die, dbg, err);
            ret.size = get_type_size(type_die, dbg, err);
        }
        if (type_tag == DW_TAG_typedef) {
            ret.name = get_type_name(type_die, dbg, err);
            ret.size = get_typedef_size_recursive(type_die, dbg, err);
        }
        if (type_tag == DW_TAG_const_type) {
            Dwarf_Attribute type_attr = nullptr;
            if (dwarf_attr(type_die, DW_AT_type, &type_attr, &err) == DW_DLV_OK) {
                Dwarf_Off type_ref;
                if (dwarf_global_formref(type_attr, &type_ref, &err) == DW_DLV_OK) {
                    Dwarf_Die const_type_die;
                    if (dwarf_offdie_b(dbg, type_ref, true, &const_type_die, &err) != DW_DLV_OK) {
                        dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
                    }
                    auto underlying = get_type_info(const_type_die, dbg, err);
                    ret.size = underlying.size;
                    ret.name = "const " + underlying.name;
                    dwarf_dealloc(dbg, const_type_die, DW_DLA_DIE);
                }
            } else {
                throw DwarfException("failed to read type attr");
            }
            dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
        }
        return ret;
    }

    using NameTypeinfoMap = std::unordered_map<std::string, InternalTypeInfo>;
    using NameLocMap = std::unordered_map<std::string, VariableLocation>;

    InternalTypeInfo read_type_attribute(
        Dwarf_Die var_die,
        Dwarf_Debug dbg,
        Dwarf_Error err,
        Dwarf_Half tag
        ) {
        Dwarf_Attribute type_attr;
        InternalTypeInfo res;
        if (dwarf_attr(var_die, DW_AT_type, &type_attr, &err) == DW_DLV_OK) {
            Dwarf_Off type_ref;
            if (dwarf_global_formref(type_attr, &type_ref, &err) == DW_DLV_OK) {
                Dwarf_Die type_die;
                if (dwarf_offdie_b(dbg, type_ref, true, &type_die, &err) != DW_DLV_OK) {
                    dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
                    throw DwarfException("failed to get type node");
                }
                try {
                    res = get_type_info(type_die, dbg, err);
                } catch (const DwarfException& e){
                    // TODO: logging
                }
                dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
            }
            dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
        }
        return res;
    }

    const uint8_t* decode_uleb128(const uint8_t* ptr, const uint8_t* end, Dwarf_Unsigned* val) {
        *val = 0;
        int shift = 0;
        
        while (ptr < end) {
            uint8_t byte = *ptr++;
            *val |= (byte & 0x7f) << shift;
            shift += 7;
            if ((byte & 0x80) == 0) break;
        }
        
        return ptr;
    }

    const uint8_t* decode_sleb128(const uint8_t* ptr, const uint8_t* end, Dwarf_Signed* val) {
        *val = 0;
        int shift = 0;
        uint8_t byte;
        
        do {
            if (ptr >= end) break;
            byte = *ptr++;
            *val |= (byte & 0x7f) << shift;
            shift += 7;
        } while (byte & 0x80);
        
        if (shift < sizeof(*val) * 8 && (byte & 0x40)) {
            *val |= -((Dwarf_Signed)1 << shift);
        }
        
        return ptr;
    }


    VariableLocation read_location(
        Dwarf_Ptr expr_block, 
        Dwarf_Unsigned expr_len
    ) {
        std::ostringstream oss;
        const uint8_t* ptr = static_cast<const uint8_t*>(expr_block);
        const uint8_t* end = ptr + expr_len;
        VariableLocation res;
        while (ptr < end) {
            uint8_t op = *ptr++;
            if (op == DW_OP_fbreg) {
                Dwarf_Signed offset;
                ptr = decode_sleb128(ptr, end, &offset);
                res.type = LocationType::FRAME_OFFSET;
                res.value = offset;
                break;
            }
            if (op == DW_OP_addr) {
                Dwarf_Addr addr;
                ptr = decode_uleb128(ptr, end, &addr);
                res.type = LocationType::MEMORY;
                res.value = addr;
                break;
            }
            throw DwarfException("Unsupported locspec " + std::to_string(op));
        }
        return res;
    }

    VariableLocation read_loc_attribute(
        Dwarf_Die var_die,
        Dwarf_Debug dbg,
        Dwarf_Error err
        ) {
        Dwarf_Attribute loc_attr;
        VariableLocation res;
        if (dwarf_attr(var_die, DW_AT_location, &loc_attr, &err) == DW_DLV_OK) {
            Dwarf_Locdesc* locdescs = nullptr;
            Dwarf_Signed locdesc_count = 0;
            Dwarf_Half form;
            dwarf_whatform(loc_attr, &form, &err);
            if (form == DW_FORM_exprloc) {
                Dwarf_Unsigned expr_len;
                Dwarf_Ptr expr_block;
                if (dwarf_formexprloc(loc_attr, &expr_len, &expr_block, &err) != DW_DLV_OK) {
                    dwarf_dealloc(dbg, loc_attr, DW_DLA_ATTR);
                    throw DwarfException("failed to get expression location");
                }
                res = read_location(expr_block, expr_len);
            } else {
                dwarf_dealloc(dbg, loc_attr, DW_DLA_ATTR);
                throw DwarfException("Unsupported locspec");
            }
            dwarf_dealloc(dbg, loc_attr, DW_DLA_ATTR);
        } else {
            throw DwarfException("no location attribute for variable found\n");
        }
        return res;
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

void visit_die(
    Dwarf_Die d,
    Dwarf_Debug dbg,
    Dwarf_Error err,
    uint64_t pc,
    NameTypeinfoMap& name_to_typeinfo,
    NameLocMap& var_locs
    ) {
    char *die_text;
    if (dwarf_diename(d, &die_text, &err) != DW_DLV_OK) {
        die_text = nullptr;
    }
    Dwarf_Half tag;
    if (dwarf_tag(d, &tag, &err) != DW_DLV_OK) {
        return;
    }
    Dwarf_Addr low_pc = 0;
    Dwarf_Addr high_pc = 0;
    Dwarf_Half high_pc_form = 0;
    enum Dwarf_Form_Class form_class;
    if (dwarf_lowpc(d, &low_pc, &err) == DW_DLV_OK) {
        if (dwarf_highpc_b(d, &high_pc, &high_pc_form, &form_class, &err) == DW_DLV_OK) {
            if (high_pc_form == DW_FORM_data8 && form_class == DW_FORM_CLASS_CONSTANT) {
                if (pc < low_pc || pc > low_pc + high_pc) {
                    return;
                }
            }
        }
    }
    if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
        if (die_text) {
            try {
                auto type_info = read_type_attribute(d, dbg, err, tag);
                name_to_typeinfo.emplace(die_text, type_info);
                VariableLocation loc = read_loc_attribute(d, dbg, err);
                var_locs.emplace(die_text, loc);
            } catch (const DwarfException& e) {
                // TODO: logging
            }
        }
    }
    dwarf_dealloc(dbg, die_text, DW_DLA_STRING);

    Dwarf_Die next;
    if (dwarf_child(d, &next, &err) != DW_DLV_OK) {
        return;
    }
    visit_die(next, dbg, err, pc, name_to_typeinfo, var_locs);
    Dwarf_Die sib;
    while(dwarf_siblingof_b(dbg, next, true, &sib, &err) == DW_DLV_OK) {
        dwarf_dealloc(dbg, next, DW_DLA_DIE);
        visit_die(sib, dbg, err, pc, name_to_typeinfo, var_locs);
        next = sib;
    }
    dwarf_dealloc(dbg, sib, DW_DLA_DIE);
}

std::vector<VariableInfo> DebugInfoProvider::get_available_variables(uint64_t pc) const {
    std::vector<VariableInfo> res;

    Dwarf_Bool      is_info = true;
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
    NameTypeinfoMap name_typeinfo;
    NameLocMap var_locs;
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
        visit_die(cu_die, dbg, err, pc, name_typeinfo, var_locs);
        ++cu_cnt;
    }
    for (auto& [name, loc] : var_locs) {
        VariableInfo info;
        info.location = loc;
        info.name = name;
        auto typeinfo_it = name_typeinfo.find(name);
        if (typeinfo_it == name_typeinfo.end()) {
            info.type_name = "unknown";
            info.size = -1;
            res.emplace_back(std::move(info));
            continue;
        }
        info.type_name = typeinfo_it->second.name;
        info.size = typeinfo_it->second.size;
        res.emplace_back(std::move(info));
    }
    return res;
}

DebugInfoProvider::~DebugInfoProvider() {
    if (!empty()) {
        dwarf_finish(dbg, &err);
        elf_end(elf_handler);
        close(elf_fd);
    }
}

