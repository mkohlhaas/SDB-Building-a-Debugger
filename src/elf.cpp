#include <fcntl.h>
#include <libsdb/bit.hpp>
#include <libsdb/elf.hpp>
#include <libsdb/error.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

sdb::elf::elf(const std::filesystem::path &path)
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

    void *ret;
    if ((ret = mmap(0, file_size_, PROT_READ, MAP_SHARED, fd_, 0)) == MAP_FAILED)
    {
        close(fd_);
        error::send_errno("could not mmap ELF file");
    }

    data_ = reinterpret_cast<std::byte *>(ret);

    std::copy(data_, data_ + sizeof(header_), as_bytes(header_));

    parse_section_headers();
}

sdb::elf::~elf()
{
    munmap(data_, file_size_);
    close(fd_);
}

void
sdb::elf::parse_section_headers()
{
    auto n_headers = header_.e_shnum;
    if (n_headers == 0 and header_.e_shentsize != 0)
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
