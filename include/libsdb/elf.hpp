#pragma once

#include <elf.h>
#include <filesystem>
#include <vector>

namespace sdb
{
    using namespace std::filesystem;

    class elf
    {

      public:
        elf(const class path &path);

        ~elf();

        elf(const elf &)            = delete;
        elf &operator=(const elf &) = delete;

        class path
        path() const
        {
            return path_;
        }

        const Elf64_Ehdr &
        get_header() const
        {
            return header_;
        }

      private:
        void parse_section_headers();

        using vec_sec_headers = std::vector<Elf64_Shdr>;

        int             fd_;
        class path      path_;
        std::size_t     file_size_;
        std::byte      *data_;
        Elf64_Ehdr      header_;
        vec_sec_headers section_headers_;
    };
} // namespace sdb
