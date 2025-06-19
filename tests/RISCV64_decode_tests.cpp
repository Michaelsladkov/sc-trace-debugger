#include "RISCV64_decode.hpp"

#include <gtest/gtest.h>

namespace {

void check_instruction(const RISCV64Decode::Instruction& to_test, const RISCV64Decode::Instruction& ref) {
    ASSERT_EQ(to_test.type, ref.type);
    switch (ref.type)
    {
    case RISCV64Decode::InstructionType::LOAD: {
        auto& test_load = to_test.content.loadContent;
        auto& ref_load = ref.content.loadContent;
        ASSERT_EQ(test_load.extendSign, ref_load.extendSign);
        ASSERT_EQ(test_load.offset, ref_load.offset);
        ASSERT_EQ(test_load.rdIndex, ref_load.rdIndex);
        ASSERT_EQ(test_load.rs1Index, ref_load.rs1Index);
        ASSERT_EQ(test_load.size, ref_load.size);
        break;
    }
    case RISCV64Decode::InstructionType::STORE: {
        auto& test_store = to_test.content.storeContent;
        auto& ref_store = ref.content.storeContent;
        ASSERT_EQ(test_store.offset, ref_store.offset);
        ASSERT_EQ(test_store.rs1Index, ref_store.rs1Index);
        ASSERT_EQ(test_store.rs2Index, ref_store.rs2Index);
        ASSERT_EQ(test_store.size, ref_store.size);
    }
    default:
        break;
    }
}

}

// Reference opcodes obtained from https://luplab.gitlab.io/rvcodecjs/

TEST(LoadDecode, ld_no_offset) {
    uint32_t instr = 0x0005b503; // ld x10, 0(x11)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::LOAD;
    ref.content.loadContent.extendSign = true;
    ref.content.loadContent.offset = 0;
    ref.content.loadContent.rdIndex = 10;
    ref.content.loadContent.rs1Index = 11;
    ref.content.loadContent.size = 8;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(LoadDecode, lh_positive_offset) {
    uint32_t instr = 0x04039183; // lh x3, 64(x7)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::LOAD;
    ref.content.loadContent.extendSign = true;
    ref.content.loadContent.offset = 64;
    ref.content.loadContent.rdIndex = 3;
    ref.content.loadContent.rs1Index = 7;
    ref.content.loadContent.size = 2;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(LoadDecode, lb_negative_offset) {
    uint32_t instr = 0xfdbf8b83; // lb x23, -37(x31)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::LOAD;
    ref.content.loadContent.extendSign = true;
    ref.content.loadContent.offset = -37;
    ref.content.loadContent.rdIndex = 23;
    ref.content.loadContent.rs1Index = 31;
    ref.content.loadContent.size = 1;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(LoadDecode, lwu_negative_offset) {
    uint32_t instr = 0xdff56303; // lwu x6, -513(x10)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::LOAD;
    ref.content.loadContent.extendSign = false;
    ref.content.loadContent.offset = -513;
    ref.content.loadContent.rdIndex = 6;
    ref.content.loadContent.rs1Index = 10;
    ref.content.loadContent.size = 4;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(StoreDecode, sb_no_offset) {
    uint32_t instr = 0x00628023; // sb x6, 0(x5)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::STORE;
    ref.content.storeContent.offset = 0;
    ref.content.storeContent.rs1Index = 5;
    ref.content.storeContent.rs2Index = 6;
    ref.content.storeContent.size = 1;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(StoreDecode, sh_negative_offset) {
    uint32_t instr = 0xfe629fa3; // sh x6, -1(x5)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::STORE;
    ref.content.storeContent.offset = -1;
    ref.content.storeContent.rs1Index = 5;
    ref.content.storeContent.rs2Index = 6;
    ref.content.storeContent.size = 2;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(StoreDecode, sw_positive_offset) {
    uint32_t instr = 0x7e1f2fa3; // sw x1, 2047(x30)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::STORE;
    ref.content.storeContent.offset = 2047;
    ref.content.storeContent.rs1Index = 30;
    ref.content.storeContent.rs2Index = 1;
    ref.content.storeContent.size = 4;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(StoreDecode, sd_negative_offset) {
    uint32_t instr = 0x8179b0a3; // sd x23, -2049(x19)
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::STORE;
    ref.content.storeContent.offset = -2047;
    ref.content.storeContent.rs1Index = 19;
    ref.content.storeContent.rs2Index = 23;
    ref.content.storeContent.size = 8;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(Unsupported, illegal) {
    uint32_t instr = 0;
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::UNSUPPORTED;
    check_instruction(RISCV64Decode::decode(instr), ref);
}

TEST(Unsupported, arithmetics) {
    uint32_t instr = 0x003080b3; // add x0, x1, x3
    RISCV64Decode::Instruction ref;
    ref.type = RISCV64Decode::InstructionType::UNSUPPORTED;
    check_instruction(RISCV64Decode::decode(instr), ref);
}
