extern "C" {
#include "mir.h"
#include "mir-gen.h"
}

#include "prot/jit/base.hh"

namespace prot::engine {
namespace {

#define PROT_MIR_R_OP(OP, MIR_OP)                                              \
  case k##OP: {                                                                \
    loadReg(rs1_reg, insn.rs1());                                              \
    loadReg(rs2_reg, insn.rs2());                                              \
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
    loadReg(rs1_reg, insn.rs1());                                              \
                                                                               \
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
                                                                               \
    loadReg(rs1_reg, insn.rs1());                                              \
    loadReg(rs2_reg, insn.rs2());                                              \
                                                                               \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, COND, MIR_new_label_op(ctx, true_label), \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_reg_op(ctx, rs2_reg)));               \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(                                                          \
            ctx, MIR_MOV, MIR_new_reg_op(ctx, pc_reg),                         \
            MIR_new_int_op(ctx, static_cast<int64_t>(static_cast<uint32_t>(    \
                                    curPC + isa::kWordSize)))));               \
                                                                               \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(ctx, MIR_JMP, MIR_new_label_op(ctx, end_label)));         \
                                                                               \
    MIR_append_insn(ctx, func_item, true_label);                               \
                                                                               \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(                                                          \
            ctx, MIR_MOV, MIR_new_reg_op(ctx, pc_reg),                         \
            MIR_new_int_op(ctx, static_cast<int64_t>(static_cast<uint32_t>(    \
                                    curPC + insn.imm())))));                   \
    MIR_append_insn(ctx, func_item, end_label);                                \
    break;                                                                     \
  }

#define PROT_MIR_LOAD(OP, MEM_TYPE, EXT_INSN)                                  \
  case k##OP: {                                                                \
    loadReg(rs1_reg, insn.rs1());                                              \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_ADDS, MIR_new_reg_op(ctx, rs1_reg),  \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_int_op(ctx, insn.imm())));            \
    getHostAddr(host_addr, rs1_reg);                                           \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, val_reg),               \
                     MIR_new_mem_op(ctx, MEM_TYPE, 0, host_addr, 0, 0)));      \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, EXT_INSN, MIR_new_reg_op(ctx, ext_reg),  \
                                 MIR_new_reg_op(ctx, val_reg)));               \
    setDst(insn.rd(), MIR_new_reg_op(ctx, ext_reg));                           \
    break;                                                                     \
  }

#define PROT_MIR_LOAD_NOEXT(OP, MEM_TYPE)                                      \
  case k##OP: {                                                                \
    loadReg(rs1_reg, insn.rs1());                                              \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_ADDS, MIR_new_reg_op(ctx, rs1_reg),  \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_int_op(ctx, insn.imm())));            \
    getHostAddr(host_addr, rs1_reg);                                           \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, val_reg),               \
                     MIR_new_mem_op(ctx, MEM_TYPE, 0, host_addr, 0, 0)));      \
    setDst(insn.rd(), MIR_new_reg_op(ctx, val_reg));                           \
    break;                                                                     \
  }

#define PROT_MIR_STORE(OP, MEM_TYPE)                                           \
  case k##OP: {                                                                \
    loadReg(rs1_reg, insn.rs1());                                              \
    MIR_append_insn(ctx, func_item,                                            \
                    MIR_new_insn(ctx, MIR_ADDS, MIR_new_reg_op(ctx, rs1_reg),  \
                                 MIR_new_reg_op(ctx, rs1_reg),                 \
                                 MIR_new_int_op(ctx, insn.imm())));            \
    getHostAddr(host_addr, rs1_reg);                                           \
    loadReg(rs2_reg, insn.rs2());                                              \
    MIR_append_insn(                                                           \
        ctx, func_item,                                                        \
        MIR_new_insn(ctx, MIR_MOV,                                             \
                     MIR_new_mem_op(ctx, MEM_TYPE, 0, host_addr, 0, 0),        \
                     MIR_new_reg_op(ctx, rs2_reg)));                           \
    break;                                                                     \
  }

using JitFunction = void (*)(CPUState &);

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

class MIRJit : public Translator {
public:
  MIRJit() : ctx(MIR_init()) {
    MIR_gen_init(ctx);
    MIR_load_external(ctx, "syscallHelper",
                      reinterpret_cast<void *>(syscallHelper));
  }

  ~MIRJit() override {
    MIR_gen_finish(ctx);
    MIR_finish(ctx);
  }

private:
  [[nodiscard]] JitFunction translate(const BBInfo &info) override;

  MIR_context_t ctx;
};

JitFunction MIRJit::translate(const BBInfo &info) {
  MIR_module_t module = MIR_new_module(ctx, "jit_module");

  MIR_var_t func_args[] = {{MIR_T_P, "state", 0}};
  MIR_item_t func_item =
      MIR_new_func_arr(ctx, "jit_func", 0, nullptr, 1, func_args);

  MIR_func_t func = func_item->u.func;

  MIR_reg_t state_ptr = MIR_reg(ctx, "state", func);
  MIR_reg_t pc_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "pc");
  MIR_reg_t rs1_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "rs1");
  MIR_reg_t rs2_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "rs2");
  MIR_reg_t rd_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "rd");

  MIR_reg_t mem_base_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "mem_base");

  MIR_reg_t host_addr = MIR_new_func_reg(ctx, func, MIR_T_I64, "host_addr");

  MIR_reg_t val_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "val");
  MIR_reg_t ext_reg = MIR_new_func_reg(ctx, func, MIR_T_I64, "ext");

  MIR_reg_t ch_pc = MIR_new_func_reg(ctx, func, MIR_T_I64, "ch_pc");
  MIR_reg_t ch_off = MIR_new_func_reg(ctx, func, MIR_T_I64, "ch_off");
  MIR_reg_t ch_base = MIR_new_func_reg(ctx, func, MIR_T_I64, "ch_base");
  MIR_reg_t ch_entry = MIR_new_func_reg(ctx, func, MIR_T_I64, "ch_entry");
  MIR_reg_t ch_gpa = MIR_new_func_reg(ctx, func, MIR_T_I64, "ch_gpa");
  MIR_reg_t ch_fn = MIR_new_func_reg(ctx, func, MIR_T_I64, "ch_fn");

  auto getReg = [this, state_ptr](auto regId) {
    return MIR_new_mem_op(ctx, MIR_T_U32,
                          offsetof(CPUState, regs) + isa::kWordSize * regId,
                          state_ptr, 0, 0);
  };

  auto loadReg = [this, func_item, getReg](auto reg, auto regId) {
    if (regId == 0)
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, reg),
                                   MIR_new_int_op(ctx, 0)));
    else
      MIR_append_insn(
          ctx, func_item,
          MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, reg), getReg(regId)));
  };

  auto getPC = [this, state_ptr]() {
    return MIR_new_mem_op(ctx, MIR_T_U32, offsetof(CPUState, pc), state_ptr, 0,
                          0);
  };

  auto getMemBase = [this, state_ptr]() {
    return MIR_new_mem_op(ctx, MIR_T_P, offsetof(CPUState, mem_base), state_ptr,
                          0, 0);
  };
  auto setDst = [this, func_item, getReg](auto dstId, auto dst_op) {
    if (dstId != 0) {
      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_MOV, getReg(dstId), dst_op));
    }
  };

  auto getHostAddr = [this, func_item, mem_base_reg](auto host_addr,
                                                     auto guest_addr_reg) {
    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_UEXT32,
                                 MIR_new_reg_op(ctx, host_addr),
                                 MIR_new_reg_op(ctx, guest_addr_reg)));

    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, host_addr),
                                 MIR_new_reg_op(ctx, mem_base_reg),
                                 MIR_new_reg_op(ctx, host_addr)));
  };

  MIR_append_insn(ctx, func_item,
                  MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, mem_base_reg),
                               getMemBase()));

  isa::Addr curPC = info.startPC;

  for (const auto &insn : info.insns) {
    const bool isLast = (&insn == &info.insns.back());

    switch (insn.opcode()) {
      using enum isa::Opcode;

      PROT_MIR_ALU_OP(ADD, MIR_ADDS)
      PROT_MIR_ALU_OP(AND, MIR_ANDS)
      PROT_MIR_ALU_OP(OR, MIR_ORS)
      PROT_MIR_ALU_OP(XOR, MIR_XORS)

      PROT_MIR_ALU_OP(SLL, MIR_LSHS)
      PROT_MIR_ALU_OP(SRL, MIR_URSHS)
      PROT_MIR_ALU_OP(SRA, MIR_RSHS)

      PROT_MIR_R_OP(SUB, MIR_SUBS)

      PROT_MIR_R_OP(SLT, MIR_LTS)
      PROT_MIR_R_OP(SLTU, MIR_ULTS)

      PROT_MIR_I_OP(SLTI, MIR_LTS)
      PROT_MIR_I_OP(SLTIU, MIR_ULTS)

      PROT_MIR_B_COND_OP(BEQ, MIR_BEQS)
      PROT_MIR_B_COND_OP(BNE, MIR_BNES)
      PROT_MIR_B_COND_OP(BLT, MIR_BLTS)
      PROT_MIR_B_COND_OP(BGE, MIR_BGES)
      PROT_MIR_B_COND_OP(BLTU, MIR_UBLTS)
      PROT_MIR_B_COND_OP(BGEU, MIR_UBGES)

      PROT_MIR_LOAD_NOEXT(LW, MIR_T_U32)
      PROT_MIR_LOAD(LH, MIR_T_I16, MIR_EXT16)
      PROT_MIR_LOAD(LHU, MIR_T_U16, MIR_UEXT16)
      PROT_MIR_LOAD(LB, MIR_T_I8, MIR_EXT8)
      PROT_MIR_LOAD(LBU, MIR_T_U8, MIR_UEXT8)

      PROT_MIR_STORE(SW, MIR_T_U32)
      PROT_MIR_STORE(SH, MIR_T_U16)
      PROT_MIR_STORE(SB, MIR_T_U8)

    case kJAL: {
      MIR_append_insn(
          ctx, func_item,
          MIR_new_insn(
              ctx, MIR_MOV, MIR_new_reg_op(ctx, rd_reg),
              MIR_new_int_op(ctx, static_cast<int64_t>(static_cast<uint32_t>(
                                      curPC + isa::kWordSize)))));
      setDst(insn.rd(), MIR_new_reg_op(ctx, rd_reg));

      if (isLast) {
        MIR_append_insn(
            ctx, func_item,
            MIR_new_insn(
                ctx, MIR_MOV, MIR_new_reg_op(ctx, pc_reg),
                MIR_new_int_op(ctx, static_cast<int64_t>(static_cast<uint32_t>(
                                        curPC + insn.imm())))));
      }
      break;
    }

    case kJALR: {
      loadReg(rs1_reg, insn.rs1());

      MIR_append_insn(
          ctx, func_item,
          MIR_new_insn(
              ctx, MIR_MOV, MIR_new_reg_op(ctx, rd_reg),
              MIR_new_int_op(ctx, static_cast<int64_t>(static_cast<uint32_t>(
                                      curPC + isa::kWordSize)))));

      MIR_append_insn(ctx, func_item,
                      MIR_new_insn(ctx, MIR_ADDS, MIR_new_reg_op(ctx, pc_reg),
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
      MIR_append_insn(
          ctx, func_item,
          MIR_new_insn(
              ctx, MIR_MOV, MIR_new_reg_op(ctx, rs1_reg),
              MIR_new_int_op(ctx, static_cast<int64_t>(static_cast<uint32_t>(
                                      curPC + insn.imm())))));
      setDst(insn.rd(), MIR_new_reg_op(ctx, rs1_reg));
      break;
    }

    case kECALL: {
      MIR_var_t syscall_args[] = {{MIR_T_P, "state", 0}};
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

    if (insn.opcode() == isa::Opcode::kJAL && !isLast) {
      curPC = curPC + insn.imm();
    } else if (!isa::changesPC(insn.opcode())) {
      curPC += isa::kWordSize;
    }
  }

  if (info.insns.empty() || !isa::changesPC(info.insns.back().opcode())) {
    MIR_append_insn(
        ctx, func_item,
        MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, pc_reg),
                     MIR_new_int_op(ctx, static_cast<int64_t>(
                                             static_cast<uint32_t>(curPC)))));
  }

  MIR_append_insn(
      ctx, func_item,
      MIR_new_insn(ctx, MIR_MOV, getPC(), MIR_new_reg_op(ctx, pc_reg)));

  MIR_append_insn(
      ctx, func_item,
      MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, rd_reg),
                   MIR_new_mem_op(ctx, MIR_T_U32, offsetof(CPUState, icount),
                                  state_ptr, 0, 0)));

  MIR_append_insn(
      ctx, func_item,
      MIR_new_insn(ctx, MIR_ADDS,
                   MIR_new_mem_op(ctx, MIR_T_U32, offsetof(CPUState, icount),
                                  state_ptr, 0, 0),
                   MIR_new_reg_op(ctx, rd_reg),
                   MIR_new_int_op(ctx, info.insns.size())));

  const bool lastIsJalr =
      !info.insns.empty() && info.insns.back().opcode() == isa::Opcode::kJALR;

  if (!lastIsJalr) {
    MIR_label_t end_label = MIR_new_label(ctx);

    // if (hasEcall) {
    //   MIR_append_insn(ctx, func_item,
    //                   MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, ch_fin),
    //                                MIR_new_mem_op(ctx, MIR_T_U8,
    //                                               offsetof(CPUState, finished),
    //                                               state_ptr, 0, 0)));
    //   MIR_append_insn(ctx, func_item,
    //                   MIR_new_insn(ctx, MIR_BT,
    //                                MIR_new_label_op(ctx, end_label),
    //                                MIR_new_reg_op(ctx, ch_fin)));
    // }

    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_UEXT32, MIR_new_reg_op(ctx, ch_pc),
                                 MIR_new_reg_op(ctx, pc_reg)));
    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_URSH, MIR_new_reg_op(ctx, ch_off),
                                 MIR_new_reg_op(ctx, ch_pc),
                                 MIR_new_int_op(ctx, kTbCacheGranularityLog2)));
    MIR_append_insn(
        ctx, func_item,
        MIR_new_insn(ctx, MIR_AND, MIR_new_reg_op(ctx, ch_off),
                     MIR_new_reg_op(ctx, ch_off),
                     MIR_new_int_op(ctx, static_cast<int64_t>(kTbCacheMask))));
    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_LSH, MIR_new_reg_op(ctx, ch_off),
                                 MIR_new_reg_op(ctx, ch_off),
                                 MIR_new_int_op(ctx, 4)));
    MIR_append_insn(
        ctx, func_item,
        MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, ch_base),
                     MIR_new_mem_op(ctx, MIR_T_P,
                                    offsetof(CPUState, tb_cache_base),
                                    state_ptr, 0, 0)));
    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_ADD, MIR_new_reg_op(ctx, ch_entry),
                                 MIR_new_reg_op(ctx, ch_base),
                                 MIR_new_reg_op(ctx, ch_off)));
    MIR_append_insn(
        ctx, func_item,
        MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, ch_gpa),
                     MIR_new_mem_op(ctx, MIR_T_U32, offsetof(TbCacheEntry, gpa),
                                    ch_entry, 0, 0)));
    MIR_append_insn(
        ctx, func_item,
        MIR_new_insn(ctx, MIR_MOV, MIR_new_reg_op(ctx, ch_fn),
                     MIR_new_mem_op(ctx, MIR_T_P, 0, ch_entry, 0, 0)));
    MIR_append_insn(ctx, func_item,
                    MIR_new_insn(ctx, MIR_BNE, MIR_new_label_op(ctx, end_label),
                                 MIR_new_reg_op(ctx, ch_gpa),
                                 MIR_new_reg_op(ctx, ch_pc)));
    MIR_append_insn(
        ctx, func_item,
        MIR_new_insn(ctx, MIR_MOV,
                     MIR_new_mem_op(ctx, MIR_T_P, offsetof(CPUState, next_tb),
                                    state_ptr, 0, 0),
                     MIR_new_reg_op(ctx, ch_fn)));
    MIR_append_insn(ctx, func_item, end_label);
  }

  MIR_append_insn(ctx, func_item, MIR_new_ret_insn(ctx, 0));

  MIR_finish_func(ctx);
  MIR_finish_module(ctx);

  MIR_load_module(ctx, module);

  MIR_link(ctx, MIR_set_gen_interface, nullptr);

  return reinterpret_cast<JitFunction>(MIR_gen(ctx, func_item));
}

} // namespace

std::unique_ptr<Translator> makeMirJit() { return std::make_unique<MIRJit>(); }
} // namespace prot::engine
