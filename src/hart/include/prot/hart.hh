#ifndef PROT_HART_HH_INCLUDED
#define PROT_HART_HH_INCLUDED

#include <memory>

#include "prot/cpu_state.hh"
#include "prot/exec_engine.hh"
#include "prot/memory.hh"

namespace prot {

class Loader;

class Hart {

  Hart(std::unique_ptr<Memory> mem, std::unique_ptr<ExecEngine> engine);

  void load(const Loader &loader);

  void run() {
    while (!m_cpu->finished) {
      m_engine->step(*m_cpu);
    }
  }

private:
  std::unique_ptr<Memory> m_mem;
  std::unique_ptr<CPUState> m_cpu;
  std::unique_ptr<ExecEngine> m_engine;
};
} // namespace prot

#endif // PROT_HART_HH_INCLUDED
