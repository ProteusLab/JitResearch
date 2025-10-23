#ifndef PROT_HART_HH_INCLUDED
#define PROT_HART_HH_INCLUDED

#include <memory>

#include "prot/cpu_state.hh"
#include "prot/elf_loader.hh"
#include "prot/exec_engine.hh"
#include "prot/memory.hh"

namespace prot {
class Hart {
public:
  Hart(std::unique_ptr<Memory> mem, std::unique_ptr<ExecEngine> engine);

  void setSP(isa::Addr addr);

  void load(const ElfLoader &loader);

  void dump(std::ostream &ost) const;

  void setPC(isa::Addr addr);

  void run() {
    while (!m_cpu->finished) {
      m_engine->step(*m_cpu);
    }
  }

  auto getExitCode() { return m_cpu->getExitCode(); }

  auto getIcount() const { return m_cpu->icount; }

private:
  std::unique_ptr<Memory> m_mem;
  std::unique_ptr<CPUState> m_cpu;
  std::unique_ptr<ExecEngine> m_engine;
};
} // namespace prot

#endif // PROT_HART_HH_INCLUDED
