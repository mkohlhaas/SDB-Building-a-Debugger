#include <algorithm>
#include <cxxabi.h>
#include <fcntl.h>
#include <libsdb/bit.hpp>
#include <libsdb/elf.hpp>
#include <libsdb/error.hpp>
#include <optional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace sdb;

elf::elf(const std::filesystem::path &path)
{
    path_ = path;

    if ((fd_ = open(path.c_str(), O_LARGEFILE, O_RDONLY)) < 0)
    {
        error::send_errno("could not open ELF file");
    }

    struct stat stats;
    if (fstat(fd_, &stats) < 0)
    {
        error::send_errno("could not retrieve ELF file stats");
    }

    file_size_ = stats.st_size;

    // memory map file
    void *ret;
    if ((ret = mmap(0, file_size_, PROT_READ, MAP_SHARED, fd_, 0)) == MAP_FAILED)
    {
        close(fd_);
        error::send_errno("could not mmap ELF file");
    }

    data_ = reinterpret_cast<std::byte *>(ret);

    std::copy(data_, data_ + sizeof(header_), as_bytes(header_));

    parse_section_headers();
    build_section_map();
    parse_symbol_table();
    build_symbol_maps();
}

elf::~elf()
{
    munmap(data_, file_size_);
    close(fd_);
}

void
elf::parse_section_headers()
{
    auto n_headers = header_.e_shnum;
    if (n_headers == 0 and header_.e_shentsize != 0) // edge/special case
    {
        // read number of section headers from 1st section header
        n_headers = from_bytes<Elf64_Shdr>(data_ + header_.e_shoff).sh_size;
    }

    section_headers_.resize(n_headers);
    auto start_section_headers = data_ + header_.e_shoff;
    auto size_section_headers  = n_headers * sizeof(Elf64_Shdr);       // Elf64_Shdr is 64 Bytes (so it is aligned)
    std::copy(start_section_headers,                                   //
              start_section_headers + size_section_headers,            //
              reinterpret_cast<std::byte *>(section_headers_.data())); //
}

std::string_view
elf::get_section_name(std::size_t index) const
{
    auto &section = section_headers_[header_.e_shstrndx];
    return {reinterpret_cast<char *>(data_) + section.sh_offset + index};
}

void
elf::build_section_map()
{
    for (auto &section : section_headers_)
    {
        section_map_[get_section_name(section.sh_name)] = &section;
    }
}

elf::opt_section
elf::get_section(std::string_view name) const
{
    if (section_map_.count(name) == 0)
    {
        return std::nullopt;
    }
    return section_map_.at(name);
}

span<const std::byte>
elf::get_section_contents(std::string_view name) const
{
    if (auto sect = get_section(name); sect)
    {
        return {data_ + sect.value()->sh_offset, sect.value()->sh_size};
    }
    else
    {
        return {};
    }
}

// `index` = start of string
std::string_view
elf::get_string(std::size_t index) const
{
    // check .strtab and .dynstr sections
    auto opt_strtab = get_section(".strtab");
    if (!opt_strtab)
    {
        opt_strtab = get_section(".dynstr");
        if (!opt_strtab)
        {
            return "";
        }
    }

    // string_view from start of zero-terminated C-string
    return {reinterpret_cast<char *>(data_) + opt_strtab.value()->sh_offset + index};
}

const Elf64_Shdr *
elf::get_section_containing_address(file_addr file_addr) const
{
    if (file_addr.elf_file() != this)
    {
        return nullptr;
    }

    for (auto &section : section_headers_)
    {
        if (section.sh_addr <= file_addr.addr() and section.sh_addr + section.sh_size > file_addr.addr())
        {
            return &section;
        }
    }

    return nullptr;
}

const Elf64_Shdr *
elf::get_section_containing_address(virt_addr addr) const
{
    for (auto &section : section_headers_)
    {
        if (load_bias_ + section.sh_addr <= addr and load_bias_ + section.sh_addr + section.sh_size > addr)
        {
            return &section;
        }
    }

    return nullptr;
}

std::optional<file_addr>
elf::get_section_start_address(std::string_view name) const
{
    auto sect = get_section(name);

    return sect ? std::make_optional(file_addr{*this, sect.value()->sh_addr}) //
                : std::nullopt;
}

void
elf::parse_symbol_table()
{
    // check secionts .symtab and .dynsym for symbols
    auto opt_symtab = get_section(".symtab");
    if (!opt_symtab)
    {
        opt_symtab = get_section(".dynsym");
        if (!opt_symtab)
        {
            return; // no symbols available
        }
    }

    auto symtab = *opt_symtab;
    symbol_table_.resize(symtab->sh_size / symtab->sh_entsize);
    std::copy(data_ + symtab->sh_offset,                   //
              data_ + symtab->sh_offset + symtab->sh_size, //
              reinterpret_cast<std::byte *>(symbol_table_.data()));
}

void
elf::build_symbol_maps()
{
    for (auto &symbol : symbol_table_)
    {
        auto mangled_name = get_string(symbol.st_name);
        int  demangle_status;
        auto demangled_name = abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &demangle_status);
        if (demangle_status == 0)
        {
            symbol_name_map_.insert({demangled_name, &symbol});
            free(demangled_name);
        }

        symbol_name_map_.insert({mangled_name, &symbol});
        if (symbol.st_value != 0 and symbol.st_name != 0 and ELF64_ST_TYPE(symbol.st_info) != STT_TLS)
        {
            auto addr_range = std::pair(file_addr{*this, symbol.st_value},                   //
                                        file_addr{*this, symbol.st_value + symbol.st_size}); //
            symbol_addr_map_.insert({addr_range, &symbol});
        }
    }
}

std::vector<const Elf64_Sym *>
elf::get_symbols_by_name(std::string_view name) const
{
    auto [begin, end] = symbol_name_map_.equal_range(name);
    std::vector<const Elf64_Sym *> ret;
    std::transform(begin,                                   //
                   end,                                     //
                   std::back_inserter(ret),                 //
                   [](auto &pair) { return pair.second; }); //
    return ret;
}

std::optional<const Elf64_Sym *>
elf::get_symbol_at_address(file_addr file_address) const
{
    if (file_address.elf_file() != this)
    {
        return std::nullopt;
    }

    file_addr null_addr;

    auto it = symbol_addr_map_.find({file_address, null_addr});
    if (it == end(symbol_addr_map_))
    {
        return std::nullopt;
    }

    return it->second;
}

std::optional<const Elf64_Sym *>
elf::get_symbol_at_address(virt_addr virt_address) const
{
    return get_symbol_at_address(virt_address.to_file_addr(*this));
}

std::optional<const Elf64_Sym *>
elf::get_symbol_containing_address(file_addr file_address) const
{
    if (file_address.elf_file() != this or symbol_addr_map_.empty())
    {
        return std::nullopt;
    }

    file_addr null_addr;
    auto      it = symbol_addr_map_.lower_bound({file_address, null_addr});
    if (it != end(symbol_addr_map_))
    {
        if (auto [file_address_pair, symbol] = *it; file_address_pair.first == file_address)
        {
            return symbol;
        }
    }

    // if the current iterator is the begin iterator, there is no entry preceding it, so we return an empty optional
    if (it == begin(symbol_addr_map_))
    {
        return std::nullopt;
    }

    --it;

    // the symbol containing the address begins earlier than the address and spans past it
    if (auto [file_address_pair, symbol] = *it; file_address_pair.first < file_address and //
                                                file_address_pair.second > file_address)
    {
        return symbol;
    }

    return std::nullopt;
}

std::optional<const Elf64_Sym *>
elf::get_symbol_containing_address(virt_addr virt_address) const
{
    return get_symbol_containing_address(virt_address.to_file_addr(*this));
}
