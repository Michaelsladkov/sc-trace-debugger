#include "RISCV64_model.hpp"

#include <string>
#include <sstream>

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

namespace {
    class RISCV64ModelDUT : public RISCV64Model {
    public:
        void init_dut(std::istream& trace_input, const std::string& filename) {
            init(trace_input, filename);
        }
    };
}

TEST(SequenceTests, StepForAndBack) {
    std::stringstream trace;
    trace << "1 2 N 0 0 4" << std::endl;
    trace << "2 2 N 4 0 8 x3=ff" << std::endl;
    trace << "3 2 N 8 0 c" << std::endl;
    trace << "4 2 N c 0 10 x3=fe" << std::endl;
    trace << "5 2 N 10 0 14" << std::endl;
    RISCV64ModelDUT dut;
    dut.init_dut(trace, "sample text");
    dut.step_forward(); // simulate line 0
    ASSERT_EQ(0, dut.read_pc());
    auto regs = dut.get_all_regs();
    for (auto& [name, value] : regs) {
        ASSERT_EQ(0, value);
    }

    dut.step_forward();
    ASSERT_EQ(4, dut.read_pc());
    ASSERT_EQ(255, dut.read_register("x3"));
    regs = dut.get_all_regs();
    for (auto& [name, value] : regs) {
        if (name != "x3") {
            ASSERT_EQ(0, value);
        }
    }

    dut.step_forward();
    ASSERT_EQ(8, dut.read_pc());
    dut.step_forward();
    ASSERT_EQ(12, dut.read_pc());
    dut.step_forward();
    ASSERT_EQ(16, dut.read_pc());

    ASSERT_EQ(254, dut.read_register("x3"));
    regs = dut.get_all_regs();
    for (auto& [name, value] : regs) {
        if (name != "x3") {
            ASSERT_EQ(0, value);
        }
    }

    dut.step_back();
    ASSERT_EQ(12, dut.read_pc());
    ASSERT_EQ(254, dut.read_register("x3"));
    regs = dut.get_all_regs();
    for (auto& [name, value] : regs) {
        if (name != "x3") {
            ASSERT_EQ(0, value);
        }
    }

    dut.step_back();
    ASSERT_EQ(8, dut.read_pc());
    ASSERT_EQ(255, dut.read_register("x3"));
    regs = dut.get_all_regs();
    for (auto& [name, value] : regs) {
        if (name != "x3") {
            ASSERT_EQ(0, value);
        }
    }
}
