#include <cassert>
#include <libsdb/elf.hpp>
#include <libsdb/types.hpp>

using namespace sdb;

virt_addr
file_addr::to_virt_addr() const
{
    assert(elf_ && "to_virt_addr called on null address");

    auto section = elf_->get_section_containing_address(*this);

    return section ? virt_addr{addr_ + elf_->load_bias().addr()} //
                   : virt_addr{};
}

file_addr
virt_addr::to_file_addr(const elf &elf) const
{
    auto section = elf.get_section_containing_address(*this);

    return section ? file_addr{elf, addr_ - elf.load_bias().addr()} //
                   : file_addr{};
}
