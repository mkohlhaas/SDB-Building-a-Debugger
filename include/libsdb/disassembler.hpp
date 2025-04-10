#pragma once

#include <libsdb/process.hpp>
#include <optional>

namespace sdb
{
    class disassembler
    {
        struct instruction
        {
            virt_addr   address;
            std::string text;
        };

      public:
        disassembler(process &proc) : process_(&proc)
        {
        }

        using vec_instructions = std::vector<instruction>;

        // Disassemble code at `address`; default is current instruction pointer.
        vec_instructions disassemble(std::size_t n_instructions, std::optional<virt_addr> address = std::nullopt);

      private:
        process *process_;
    };
} // namespace sdb
