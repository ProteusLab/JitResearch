#include "prot/hart.hh"

namespace prot {
Hart::Hart(std::unique_ptr<Memory> mem, std::unique_ptr<ExecEngine> engine)
    : m_mem(std::move(mem)), m_cpu(std::make_unique<CPUState>(m_mem.get())),
      m_engine(std::move(engine)) {}

} // namespace prot
