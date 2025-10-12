#include "prot/hart.hh"

namespace prot {
Hart::Hart(std::unique_ptr<Memory> mem, std::unique_ptr<ExecEngine> engine)
    : m_mem(std::move(mem)), m_cpu(std::make_unique<CPUState>(m_mem.get())),
      m_engine(std::move(engine)) {}

void Hart::load(const ElfLoader &loader) {
  loader.loadMemory(*m_mem);
  setPC(loader.getEntryPoint());
}

void Hart::dump(std::ostream &ost) const { m_cpu->dump(ost); }

void Hart::setSP(isa::Addr addr) { m_cpu->setReg(2, addr); }

void Hart::setPC(isa::Addr addr) { m_cpu->setPC(addr); }
} // namespace prot
