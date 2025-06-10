#include "RISCV64_model.hpp"

#include <string>

#include <gtest/gtest.h>

TEST(LineTests, FullLine) {
    std::string line = "                1221           3 N 0000000002000348 00000193 000000000200034c x3=0000000000000000";
    TraceLine tl(line);
    ASSERT_EQ(tl.time, 1221);
    ASSERT_EQ(tl.rsv1, 3);
    ASSERT_EQ(tl.rsv2, 'N');
    ASSERT_EQ(tl.cur_pc, 0x2000348);
    ASSERT_EQ(tl.instr, 0x193);
    ASSERT_EQ(tl.next_pc, 0x200034c);
    ASSERT_TRUE(tl.changed_reg.has_value());
    ASSERT_EQ(tl.changed_reg->type, RegType::INT);
    ASSERT_EQ(tl.changed_reg->index, 3);
    ASSERT_TRUE(tl.new_reg_val.has_value());
    ASSERT_EQ(tl.new_reg_val, 0);
}

TEST(LineTests, NoReg) {
    std::string line = "                2698           0 N 00000000020004c0 0000100f 00000000020004c4";
    TraceLine tl(line);
    ASSERT_EQ(tl.time, 2698);
    ASSERT_EQ(tl.rsv1, 0);
    ASSERT_EQ(tl.rsv2, 'N');
    ASSERT_EQ(tl.cur_pc, 0x20004c0);
    ASSERT_EQ(tl.instr, 0x100f);
    ASSERT_EQ(tl.next_pc, 0x20004c4);
    ASSERT_FALSE(tl.changed_reg.has_value());
    ASSERT_FALSE(tl.new_reg_val.has_value());
}
