#include "mir.h"
#include "mir-gen.h"
#include "prot/jit/base.hh"

#include <cassert>
#include <cstdint>
#include <fmt/core.h>
#include <functional>
#include <iostream>
#include <span>
#include <sys/mman.h>

namespace prot::engine {
namespace {

#define PROT_MIR_R_OP(OP, MIR_OP)                                              \
  case k##OP: {                                                                \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),   \
                                 getReg(insn.rs1())));                         \
                                                                               \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs2_reg),   \
                                 getReg(insn.rs2())));                         \
                                                                               \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_OP, MIR_new_reg_op(ctx, rd_reg),     \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_reg_op(ctx, rs2_reg)));               \
                                                                               \
    setDst(insn.rd(), MIR_new_reg_op(ctx, rd_reg));                            \
    break;                                                                     \
  }

#define PROT_MIR_I_OP(OP, MIR_OP)                                              \
  case k##OP: {                                                                \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),   \
                                 getReg(insn.rs1())));                         \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_OP, MIR_new_reg_op(ctx, rd_reg),     \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_int_op(ctx, insn.imm())));            \
    setDst(insn.rd(), MIR_new_reg_op(ctx, rd_reg));                            \
    break;                                                                     \
  }

#define PROT_MIR_ALU_OP(OP, MIR_OP)                                            \
  PROT_MIR_R_OP(OP, MIR_OP)                                                    \
  PROT_MIR_I_OP(OP##I, MIR_OP)

#define PROT_MIR_B_COND_OP(OP, COND)                                           \
  case k##OP: {                                                                \
    MIR_label_t true_label = MIR_new_label(ctx);                               \
    MIR_label_t end_label = MIR_new_label(ctx);                                \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),   \
                                 getReg(insn.rs1())));                         \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs2_reg),   \
                                 getReg(insn.rs2())));                         \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, COND, MIR_new_label_op(ctx, true_label), \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_reg_op(ctx, rs2_reg)));               \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, pc_reg),    \
                                 MIR_new_reg_op(ctx, pc_reg),                  \
                                 MIR_new_int_op(ctx, isa::kWordSize)));        \
                                                                               \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, end_label)));         \
                                                                               \
    MIR_append_insn(ctx, func_item, true_label);                               \
                                                                               \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, pc_reg),    \
                                 MIR_new_reg_op(ctx, pc_reg),                  \
                                 MIR_new_int_op(ctx, insn.imm())));            \
    MIR_append_insn(ctx, func_item, end_label);                                \
    break;                                                                     \
  }

using JitFunction = void (*)(CPUState &);

class MIRJit : public JitEngine {
public:
  MIRJit() : ctx(MIR_init()) { MIR_gen_init(ctx); }

  ~MIRJit() override {
    MIR_gen_finish(ctx);
    MIR_finish(ctx);
  }

private:
  bool doJIT(CPUState &state) override {
    const auto pc = state.getPC();
    auto found = m_cacheTB.find(pc);
    if (found != m_cacheTB.end()) {
      found->second(state);
      return true;
    }

    const auto *bbInfo = getBBInfo(pc);
    if (bbInfo == nullptr) {
      return false;
    }

    auto func = translate(*bbInfo);
    auto [it, wasNew] = m_cacheTB.try_emplace(pc, func);
    assert(wasNew);

    it->second(state);

    return true;
  }

  [[nodiscard]] JitFunction translate(const BBInfo &info);

  MIR_context_t ctx;
  std::unordered_map<isa::Addr, JitFunction> m_cacheTB;
};

template <typename T> void storeHelper(CPUState &state, isa::Addr addr, T val) {
  state.memory->write(addr, val);
}

template <typename T> T loadHelper(CPUState &state, isa::Addr addr) {
  return state.memory->read<T>(addr);
}

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

JitFunction MIRJit::translate(const BBInfo &info) {
  MIR_module_t module = MIR_new_module(ctx, "jit_module");

  MIR_var_t func_args[] = {{MIR_T_P, "state", 0}};
  MIR_item_t func_item =
      MIR_new_func_arr(ctx, "jit_func", 0, nullptr, 1, func_args);

#if 0
  // MIR_gen_set_debug_file(ctx, stdout);
  // MIR_gen_set_debug_level(ctx, 2);
#endif

  MIR_func_t func = func_item->u.func;

  MIR_reg_t state_ptr = MIR_reg(ctx, "state", func);
  MIR_reg_t pc_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "pc");
  MIR_reg_t rs1_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "rs1");
  MIR_reg_t rs2_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "rs2");
  MIR_reg_t rd_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "rd");

  auto getReg = [this, state_ptr](auto regId) {
    return MIR_new_mem_op(ctx, MIR_T_I64,
                          offsetof(CPUState, regs) + isa::kWordSize * regId,
                          state_ptr, 0, 0);
  };

  auto getPC = [this, state_ptr]() {
    return MIR_new_mem_op(ctx, MIR_T_I64, offsetof(CPUState, pc), state_ptr, 0,
                          0);
  };

  auto setDst = [this, func_item, getReg](auto dstId, auto dst_op) {
    if (dstId != 0) {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, getReg(dstId), dst_op));
    }
  };

  // std::cout << "PC = " << std::hex << state.getPC() << std::endl;

  MIR_append_insn(
      ctx, func_item,
      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, pc_reg), getPC()));

  for (const auto &insn : info.insns) {
    switch (insn.opcode()) {
      using enum isa::Opcode;

      PROT_MIR_ALU_OP(ADD, MIR_ADD)
      PROT_MIR_ALU_OP(AND, MIR_AND)
      PROT_MIR_ALU_OP(OR, MIR_OR)
      PROT_MIR_ALU_OP(XOR, MIR_XOR)

      PROT_MIR_ALU_OP(SLL, MIR_LSH)
      PROT_MIR_ALU_OP(SRL, MIR_URSH)
      PROT_MIR_ALU_OP(SRA, MIR_RSH)

      PROT_MIR_R_OP(SUB, MIR_SUB)

      PROT_MIR_R_OP(SLT, MIR_LT)
      PROT_MIR_R_OP(SLTU, MIR_ULT)

      PROT_MIR_I_OP(SLTI, MIR_LT)
      PROT_MIR_I_OP(SLTIU, MIR_ULT)

      PROT_MIR_B_COND_OP(BEQ, MIR_BEQ)
      PROT_MIR_B_COND_OP(BNE, MIR_BNE)
      PROT_MIR_B_COND_OP(BLT, MIR_BLT)
      PROT_MIR_B_COND_OP(BLTU, MIR_ULT)
      PROT_MIR_B_COND_OP(BGE, MIR_BGE)
      PROT_MIR_B_COND_OP(BGEU, MIR_UGE)

    // PROT_MIR_L_OP
    case kLW: {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),
                                   getReg(insn.rs1())));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_int_op(ctx, insn.imm())));

      MIR_type_t load_res_types[] = {MIR_T_I64};
      MIR_var_t load_args[] = {{MIR_T_P, "state", 0}, {MIR_T_I64, "addr", 0}};
      MIR_item_t load_proto = MIR_new_proto_arr(ctx, "load_word_proto", 1,
                                                load_res_types, 2, load_args);

      MIR_append_insn(
          ctx, func_item,
          MIR_new_call_insn(
              ctx, 4, MIR_new_ref_op(ctx, load_proto),
              MIR_new_ref_op(ctx, MIR_new_import(ctx, "loadHelperWord")),
              MIR_new_reg_op(ctx, rd_reg), MIR_new_reg_op(ctx, state_ptr),
              MIR_new_reg_op(ctx, rs1_reg)));

      setDst(insn.rd(), MIR_new_reg_op(ctx, rd_reg));
      break;
    }

    // PROT_MIR_S_OP
    case kSW: {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),
                                   getReg(insn.rs1())));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_int_op(ctx, insn.imm())));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs2_reg),
                                   getReg(insn.rs2())));

      MIR_var_t store_args[] = {
          {MIR_T_P, "state", 0}, {MIR_T_I64, "addr", 0}, {MIR_T_I64, "val", 0}};
      MIR_item_t store_proto =
          MIR_new_proto_arr(ctx, "store_word_proto", 0, nullptr, 3, store_args);

      MIR_append_insn(
          ctx, func_item,
          MIR_new_call_insn(
              ctx, 5, MIR_new_ref_op(ctx, store_proto),
              MIR_new_ref_op(ctx, MIR_new_import(ctx, "storeHelperWord")),
              MIR_new_reg_op(ctx, state_ptr), MIR_new_reg_op(ctx, rs1_reg),
              MIR_new_reg_op(ctx, rs2_reg)));
      break;
    }

    case kJAL: {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rd_reg),
                                   MIR_new_reg_op(ctx, pc_reg)));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, rd_reg),
                                   MIR_new_reg_op(ctx, rd_reg),
                                   MIR_new_int_op(ctx, isa::kWordSize)));
      setDst(insn.rd(), MIR_new_reg_op(ctx, rd_reg));

      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_int_op(ctx, insn.imm())));
      break;
    }

    case kJALR: {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rd_reg),
                                   MIR_new_reg_op(ctx, pc_reg)));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, rd_reg),
                                   MIR_new_reg_op(ctx, rd_reg),
                                   MIR_new_int_op(ctx, isa::kWordSize)));

      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),
                                   getReg(insn.rs1())));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_int_op(ctx, insn.imm())));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_AND, MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_int_op(ctx, ~1)));

      setDst(insn.rd(), MIR_new_reg_op(ctx, rd_reg));
      break;
    }

    case kLUI: {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_int_op(ctx, insn.imm())));
      setDst(insn.rd(), MIR_new_reg_op(ctx, rs1_reg));
      break;
    }

    case kAUIPC: {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_reg_op(ctx, pc_reg)));
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_reg_op(ctx, rs1_reg),
                                   MIR_new_int_op(ctx, insn.imm())));
      setDst(insn.rd(), MIR_new_reg_op(ctx, rs1_reg));
      break;
    }

    case kECALL: {
      MIR_var_t syscall_args[] = {{MIR_T_P, "state", sizeof(CPUState)}};
      MIR_item_t syscall_proto =
          MIR_new_proto_arr(ctx, "syscall_proto", 0, nullptr, 1, syscall_args);

      MIR_append_insn(
          ctx, func_item,
          MIR_new_call_insn(
              ctx, 3, MIR_new_ref_op(ctx, syscall_proto),
              MIR_new_ref_op(ctx, MIR_new_import(ctx, "syscallHelper")),
              MIR_new_reg_op(ctx, state_ptr)));
      break;
    }

    case kFENCE:
    case kEBREAK:
    case kPAUSE:
    case kSBREAK:
    case kSCALL:
      break;

    case kNumOpcodes:
      throw std::invalid_argument{"Unexpected insn id"};

    default:
      break;
    }

    if (!isa::changesPC(insn.opcode())) {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_reg_op(ctx, pc_reg),
                                   MIR_new_int_op(ctx, isa::kWordSize)));
    }

#if 0
    std::cout << "mnemonic = " << insn.mnemonic() << std::endl;
#endif
  }

#if 0
  std::cout << "\n";
#endif

  MIR_append_insn(
      ctx, func_item,
      MIR_new_insn(ctx, MIR_MOV, getPC(), MIR_new_reg_op(ctx, pc_reg)));

  MIR_append_insn(ctx, func_item, MIR_new_ret_insn(ctx, 0));

  MIR_finish_func(ctx);
  MIR_finish_module(ctx);

  MIR_load_external(ctx, "loadHelperWord",
                    reinterpret_cast<void *>(loadHelper<isa::Word>));
  MIR_load_external(ctx, "storeHelperWord",
                    reinterpret_cast<void *>(storeHelper<isa::Word>));
  MIR_load_external(ctx, "syscallHelper",
                    reinterpret_cast<void *>(syscallHelper));

  MIR_load_module(ctx, module);

  MIR_link(ctx, MIR_set_gen_interface, nullptr);

  JitFunction compiled_func =
      reinterpret_cast<JitFunction>(MIR_gen(ctx, func_item));

  return compiled_func;
}

} // namespace

std::unique_ptr<ExecEngine> makeMirJit() { return std::make_unique<MIRJit>(); }
} // namespace prot::engine
