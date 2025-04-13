#pragma once

#include <elf.h>
#include <filesystem>
#include <libsdb/types.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

namespace sdb
{
    using namespace std::filesystem;

    // Elf file
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

        using opt_section   = std::optional<const Elf64_Shdr *>;     // Shdr = section header
        using span_bytes    = span<const std::byte>;
        using opt_file_addr = std::optional<file_addr>;

        std::string_view  get_section_name(std::size_t index) const; // index = section index
        std::string_view  get_string(std::size_t index) const;       // index = index inside string table
        opt_section       get_section(std::string_view name) const;
        span_bytes        get_section_contents(std::string_view name) const;
        const Elf64_Shdr *get_section_containing_address(file_addr addr) const;
        const Elf64_Shdr *get_section_containing_address(virt_addr addr) const;
        opt_file_addr     get_section_start_address(std::string_view name) const;

        virt_addr
        load_bias() const
        {
            return load_bias_;
        }

        void
        notify_loaded(virt_addr address)
        {
            load_bias_ = address;
        }

      private:
        void parse_section_headers();
        void build_section_map();

        using vec_sections     = std::vector<Elf64_Shdr>;
        using map_name_section = std::unordered_map<std::string_view, Elf64_Shdr *>;

        int              fd_;
        class path       path_;
        std::size_t      file_size_;
        std::byte       *data_; // elf file is memory mapped
        Elf64_Ehdr       header_;
        vec_sections     section_headers_;
        map_name_section section_map_;
        virt_addr        load_bias_;
    };
} // namespace sdb
