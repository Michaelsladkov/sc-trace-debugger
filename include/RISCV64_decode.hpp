#pragma once

#include <cstdint>

namespace RISCV64Decode {

enum class InstructionType {
    LOAD,
    STORE,
    UNSUPPORTED
};

struct Instruction {
    InstructionType type;
    union Content {
        struct LoadContent {
            int16_t offset;
            uint8_t size;
            uint8_t rs1Index;
            uint8_t rdIndex;
            bool extendSign;
        } loadContent;
        struct StoreContent {
            int16_t offset;
            uint8_t size;
            uint8_t rs1Index;
            uint8_t rs2Index;
        } storeContent;
    } content;
};

Instruction decode(uint32_t instr);

}
