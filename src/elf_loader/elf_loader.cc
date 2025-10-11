#include "prot/elf_loader.hh"

#include <ranges>

#include <elfio/elfio.hpp>
#include <fmt/core.h>
#include <fmt/std.h>

namespace prot {
namespace {
Memory::Perms toPerms(ELFIO::Elf_Word flags) {
  Memory::Perms res{Memory::Perms::kNone};

  if ((flags & ELFIO::PF_R) != ELFIO::Elf_Word{}) {
    res |= Memory::Perms::kRead;
  }

  if ((flags & ELFIO::PF_W) != ELFIO::Elf_Word{}) {
    res |= Memory::Perms::kWrite;
  }

  if ((flags & ELFIO::PF_X) != ELFIO::Elf_Word{}) {
    res |= Memory::Perms::kExec;
  }

  if (res == Memory::Perms::kNone) {
    throw std::invalid_argument{"Empty permissions for segment detected"};
  }

  return res;
}
} // namespace
ElfLoader::~ElfLoader() = default;

void ElfLoader::validate() const {
  if (m_elf->get_type() != ELFIO::ET_EXEC) {
    throw std::invalid_argument{"Invalud ELF type"};
  }

  if (m_elf->get_class() != ELFIO::ELFCLASS32) {
    throw std::invalid_argument{"Invalid ELF class"};
  }

  if (m_elf->get_machine() != ELFIO::EM_RISCV) {
    throw std::invalid_argument{"Invalid machine"};
  }
}

ElfLoader::ElfLoader(std::istream &stream)
    : m_elf(std::make_unique<ELFIO::elfio>()) {
  if (!m_elf->load(stream)) {
    throw std::invalid_argument{"Failed to load elf file"};
  }
  validate();
}

ElfLoader::ElfLoader(const std::filesystem::path &filename)
    : m_elf(std::make_unique<ELFIO::elfio>()) {
  if (!m_elf->load(filename)) {
    throw std::invalid_argument{
        fmt::format("Failed to load elf file {}", filename)};
  }
  validate();
}

isa::Addr ElfLoader::getEntryPoint() const { return m_elf->get_entry(); }

void ElfLoader::loadMemory(Memory &mem) const {
  for (const auto &seg :
       m_elf->segments | std::views::filter([](const auto &seg) {
         return seg->get_type() == ELFIO::PT_LOAD;
       })) {
    mem.allocateBlock(seg->get_virtual_address(), seg->get_memory_size(),
                      toPerms(seg->get_flags()));

    mem.writeBlock(
        std::as_bytes(std::span{seg->get_data(), seg->get_file_size()}),
        seg->get_virtual_address());

    mem.fillBlock(seg->get_virtual_address() + seg->get_file_size(),
                  std::byte(), seg->get_memory_size() - seg->get_file_size());
  }
}
} // namespace prot
