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
