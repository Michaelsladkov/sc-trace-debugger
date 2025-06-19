#include "RISCV64_decode.hpp"

constexpr uint8_t LOAD_OPCODE = 0b0000011;
constexpr uint8_t STORE_OPCODE = 0b0100011;

constexpr int16_t extend_sign(uint16_t src) {
    constexpr uint16_t mask = 1 << 11;
    return (src ^ mask) - mask;
}

RISCV64Decode::Instruction RISCV64Decode::decode(const uint32_t instr) {
    uint8_t opcode = instr & (0b1111111);
    Instruction ret;
    switch (opcode) {
    case (LOAD_OPCODE): {
        ret.type = InstructionType::LOAD;
        const uint16_t immediate = (instr >> 20) & 0b111111111111;
        const uint8_t funct3 = (instr >> 12) & 0b111;
        const uint8_t rdIndex = (instr >> 7) & 0b11111;
        const uint8_t rsIndex = (instr >> 15) & 0b11111;
        ret.content.loadContent.offset = extend_sign(immediate);
        ret.content.loadContent.rdIndex = rdIndex;
        ret.content.loadContent.rs1Index = rsIndex;
        ret.content.loadContent.size = 1 << (funct3 & 0b11);
        ret.content.loadContent.extendSign = (funct3 >> 2) == 0;
        break;
    }
    case (STORE_OPCODE): {
        ret.type = InstructionType::STORE;
        const uint16_t immediate = ((instr >> 7) & 0b11111) | (((instr >> 25) & 0b1111111) << 5);
        const uint8_t funct3 = (instr >> 12) & 0b111;
        const uint8_t rs1Index = (instr >> 15) & 0b11111;
        const uint8_t rs2Index = (instr >> 20) & 0b11111;
        ret.content.storeContent.offset = extend_sign(immediate);
        ret.content.storeContent.rs1Index = rs1Index;
        ret.content.storeContent.rs2Index = rs2Index;
        ret.content.storeContent.size = 1 << funct3;
        break;
    }
    default:
        ret.type = InstructionType::UNSUPPORTED;
        break;
    }
    return ret;
}
