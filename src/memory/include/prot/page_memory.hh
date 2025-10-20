#ifndef PROT_PAGE_MEMORY_HH_INCLUDED
#define PROT_PAGE_MEMORY_HH_INCLUDED

#include "memory.hh"
#include "page.hh"

#include <cstdint>
#include <stdexcept>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

namespace prot {

template <PageConfigConc PageConfigT, typename Func>
bool memWalker(isa::Addr addr, std::size_t size, Func func) {
  if (size == 0)
    return true;

  isa::Addr currentAddr = addr;
  std::size_t remainSize = size;

  while (remainSize > 0) {
    auto pageId = PageConfigT::getId(currentAddr);
    auto offset = PageConfigT::getOffset(currentAddr);
    auto chunkSize = std::min(remainSize, PageConfigT::getPageSize() - offset);

    if (!func(pageId, chunkSize, offset))
      return false;

    currentAddr += chunkSize;
    remainSize -= chunkSize;
  }

  return true;
}

template <std::uint64_t PageBits, std::uint64_t AddrBits>
class PageMemConfig final {
public:
  using PageConfigT = PageConfig<PageBits>;
  using PageT = Page<PageConfigT>;

  static constexpr std::uint64_t getPagesNum() {
    return kAddrSpace / PageConfigT::getPageSize();
  }

  static constexpr PageConfigT getPageConfig() { return kPageConf; }

private:
  static constexpr std::uint64_t kAddrBits{AddrBits};
  static constexpr std::uint64_t kAddrSpace{1ULL << AddrBits};
  static constexpr PageConfigT kPageConf{};

  static_assert(kAddrSpace >= PageConfigT::getPageSize(),
                "Total virtual address space is smaller than single page size");
};

template <typename T>
concept PageMemConfigConc = requires(T) {
  { T::getPagesNum() } -> std::same_as<std::uint64_t>;
  { T::getPageConfig() } -> std::same_as<typename T::PageConfigT>;
};

template <PageMemConfigConc PageMemConfigT> class PageMemory : public Memory {
public:
  using PageConfigT = typename PageMemConfigT::PageConfigT;
  using PageT = typename PageMemConfigT::PageT;
  using PageId = typename PageConfigT::PadeId;

  PageMemory() : m_config(PageMemConfigT::getPageConfig()) {
    m_storage.reserve(PageMemConfigT::getPagesNum());
  }

  void writeBlock(std::span<const std::byte> src, isa::Addr addr) override {
    const auto *cur = src.data();

    memWalker<PageConfigT>(
        addr, src.size(),
        [this, &cur](isa::Addr addr, size_t size, isa::Addr offset) {
          auto &page = this->getPage(addr);
          page.writeBlock(cur, offset, size);
          cur += size;
          return true;
        });
  }

  void readBlock(isa::Addr addr, std::span<std::byte> dest) const override {
    auto *cur = dest.data();

    memWalker<PageConfigT>(
        addr, dest.size(),
        [this, &cur](isa::Addr addr, size_t size, isa::Addr offset) {
          const auto &page = this->getPage(addr);
          page.readBlock(offset, size, cur);
          cur += size;
          return true;
        });
  }

private:
  PageT getPage(isa::Addr addr) const {
    auto pageId = m_config.getId(addr);
    auto found = m_storage.find(pageId);

    if (found == m_storage.end())
      throw PageFault{addr};

    return found->second;
  }

  PageT &getPage(isa::Addr addr) {
    auto pageId = m_config.getId(addr);
    auto [it, inserted] = m_storage.try_emplace(pageId);
    return it->second;
  }

  std::unordered_map<PageId, PageT> m_storage{};
  PageConfigT m_config;
};
} // namespace prot

#endif // PROT_PAGE_MEMORY_HH_INCLUDED
