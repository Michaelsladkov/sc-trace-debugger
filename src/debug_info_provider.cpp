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
        std::cerr << "dwarf cu processed: " << cu_cnt << std::endl;
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
            std::cerr << "Processing constant\n";
            Dwarf_Attribute type_attr = nullptr;
            if (dwarf_attr(type_die, DW_AT_type, &type_attr, &err) == DW_DLV_OK) {
                Dwarf_Off type_ref;
                if (dwarf_global_formref(type_attr, &type_ref, &err) == DW_DLV_OK) {
                    Dwarf_Die const_type_die;
                    if (dwarf_offdie_b(dbg, type_ref, true, &const_type_die, &err) != DW_DLV_OK) {
                        dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
                        std::cerr << "failed to read hidden type DIE\n";
                    }
                    Dwarf_Half const_type_tag;
                    dwarf_tag(const_type_die, &const_type_tag, &err);
                    std::cerr << "const type tag: " << const_type_tag << std::endl;
                }
            } else {
                std::cerr << "failed to read type attr\n";
            }
            dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
        }
        return ret;
    }

    using NameTagMap = std::unordered_map<std::string, uint16_t>;
    using TagTypeInfoMap = std::unordered_map<uint16_t, InternalTypeInfo>;

    void read_type_attribute(
        Dwarf_Die var_die,
        Dwarf_Debug dbg,
        Dwarf_Error err,
        Dwarf_Half tag,
        TagTypeInfoMap tag_to_typeinfo
        ) {
        Dwarf_Attribute type_attr;
        if (dwarf_attr(var_die, DW_AT_type, &type_attr, &err) == DW_DLV_OK) {
            Dwarf_Off type_ref;
            if (dwarf_global_formref(type_attr, &type_ref, &err) == DW_DLV_OK) {
                Dwarf_Die type_die;
                if (dwarf_offdie_b(dbg, type_ref, true, &type_die, &err) != DW_DLV_OK) {
                    std::cerr << "failed to get type node\n";
                }
                try {
                    auto info = get_type_info(type_die, dbg, err);
                    tag_to_typeinfo.emplace(tag, std::move(info));
                } catch (const DwarfException& e){
                    // TODO: logging
                }
                dwarf_dealloc(dbg, type_die, DW_DLA_DIE);
            }
            dwarf_dealloc(dbg, type_attr, DW_DLA_ATTR);
        }
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


    void decode_location_expression(
        Dwarf_Ptr expr_block, 
        Dwarf_Unsigned expr_len,
        bool verbose = true
    ) {
        std::ostringstream oss;
        const uint8_t* ptr = static_cast<const uint8_t*>(expr_block);
        const uint8_t* end = ptr + expr_len;
        
        while (ptr < end) {
            uint8_t op = *ptr++;
            
            if (verbose) {
                oss << "0x" << std::hex << static_cast<int>(op) << " ";
            }

            switch (op) {
                // Базовые операции
                case DW_OP_addr:
                    if (ptr + sizeof(Dwarf_Addr) > end) break;
                    oss << "DW_OP_addr(";
                    if (verbose) {
                        Dwarf_Addr addr;
                        std::memcpy(&addr, ptr, sizeof(Dwarf_Addr));
                        oss << "0x" << std::hex << addr;
                    }
                    oss << ")";
                    ptr += sizeof(Dwarf_Addr);
                    break;
                    
                case DW_OP_deref:
                    oss << "DW_OP_deref";
                    break;
                    
                case DW_OP_const1u:
                    oss << "DW_OP_const1u(" << static_cast<int>(*ptr++) << ")";
                    break;
                    
                case DW_OP_const1s:
                    oss << "DW_OP_const1s(" << static_cast<int>(*ptr++) << ")";
                    break;
                    
                case DW_OP_const2u: {
                    uint16_t val;
                    memcpy(&val, ptr, 2);
                    oss << "DW_OP_const2u(" << val << ")";
                    ptr += 2;
                    break;
                }
                
                // Операции с регистрами
                case DW_OP_reg0: case DW_OP_reg1: case DW_OP_reg2: case DW_OP_reg3:
                case DW_OP_reg4: case DW_OP_reg5: case DW_OP_reg6: case DW_OP_reg7:
                case DW_OP_reg8: case DW_OP_reg9: case DW_OP_reg10: case DW_OP_reg11:
                case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14: case DW_OP_reg15:
                case DW_OP_reg16: case DW_OP_reg17: case DW_OP_reg18: case DW_OP_reg19:
                case DW_OP_reg20: case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23:
                case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26: case DW_OP_reg27:
                case DW_OP_reg28: case DW_OP_reg29: case DW_OP_reg30: case DW_OP_reg31:
                    oss << "DW_OP_reg" << (op - DW_OP_reg0);
                    break;
                    
                case DW_OP_regx: {
                    Dwarf_Unsigned reg;
                    ptr = decode_uleb128(ptr, end, &reg);
                    oss << "DW_OP_regx(" << reg << ")";
                    break;
                }
                
                // Операции со стеком
                case DW_OP_dup:
                    oss << "DW_OP_dup";
                    break;
                    
                case DW_OP_drop:
                    oss << "DW_OP_drop";
                    break;
                    
                case DW_OP_over:
                    oss << "DW_OP_over";
                    break;
                    
                // Арифметические операции
                case DW_OP_plus:
                    oss << "DW_OP_plus";
                    break;
                    
                case DW_OP_minus:
                    oss << "DW_OP_minus";
                    break;
                    
                case DW_OP_mul:
                    oss << "DW_OP_mul";
                    break;
                    
                // Операции с frame base
                case DW_OP_fbreg: {
                    Dwarf_Signed offset;
                    ptr = decode_sleb128(ptr, end, &offset);
                    oss << "DW_OP_fbreg(" << offset << ")";
                    break;
                }
                
                // Специальные операции
                case DW_OP_piece: {
                    Dwarf_Unsigned size;
                    ptr = decode_uleb128(ptr, end, &size);
                    oss << "DW_OP_piece(" << size << ")";
                    break;
                }
                
                default:
                    oss << "DW_OP_unknown(0x" << std::hex << static_cast<int>(op) << ")";
                    break;
            }
            
            if (ptr < end) oss << ", ";
        }
        
        if (ptr != end) {
            oss << "\n[WARNING: Incomplete expression decoding]";
        }
        std::cerr << oss.str() << std::endl;
    }

    void read_loc_attribute(
        Dwarf_Die var_die,
        Dwarf_Debug dbg,
        Dwarf_Error err
        ) {
        Dwarf_Attribute loc_attr;
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
                decode_location_expression(expr_block, expr_len);
            } else {
                dwarf_dealloc(dbg, loc_attr, DW_DLA_ATTR);
                throw DwarfException("Unsupported locspec");
            }
            dwarf_dealloc(dbg, loc_attr, DW_DLA_ATTR);
        } else {
            std::cerr << ("no location attribute for variable found\n");
        }
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

void visit_die(Dwarf_Die d, Dwarf_Debug dbg, Dwarf_Error err, NameTagMap& name_to_typetag, TagTypeInfoMap& tag_to_typeinfo) {
    char *die_text;
    if (dwarf_diename(d, &die_text, &err) != DW_DLV_OK) {
        die_text = nullptr;
    }
    Dwarf_Half tag;
    if (dwarf_tag(d, &tag, &err) != DW_DLV_OK) {
        return;
    }
    if (tag == DW_TAG_variable || tag == DW_TAG_formal_parameter) {
        if (!die_text) {
            std::cerr << "TEXT_ERROR" << std::endl;
        } else {
            std::cerr << "die text: " << die_text << " tag: " << tag << std::endl;
            name_to_typetag.emplace(die_text, tag);
        }
        read_type_attribute(d, dbg, err, tag, tag_to_typeinfo);
        read_loc_attribute(d, dbg, err);
    }
    dwarf_dealloc(dbg, die_text, DW_DLA_STRING);

    Dwarf_Die next;
    if (dwarf_child(d, &next, &err) != DW_DLV_OK) {
        return;
    }
    visit_die(next, dbg, err, name_to_typetag, tag_to_typeinfo);
    Dwarf_Die sib;
    while(dwarf_siblingof_b(dbg, next, true, &sib, &err) == DW_DLV_OK) {
        dwarf_dealloc(dbg, next, DW_DLA_DIE);
        visit_die(sib, dbg, err, name_to_typetag, tag_to_typeinfo);
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
    NameTagMap name_to_tag_map;
    TagTypeInfoMap tag_typeinfo_map;
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
        visit_die(cu_die, dbg, err, name_to_tag_map, tag_typeinfo_map);
        ++cu_cnt;
    }

    std::cerr << "processed cu num: " << cu_cnt << std::endl;
    return res;
}

DebugInfoProvider::~DebugInfoProvider() {
    if (!empty()) {
        dwarf_finish(dbg, &err);
        elf_end(elf_handler);
        close(elf_fd);
    }
}

