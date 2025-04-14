#pragma once

#include <libsdb/elf.hpp>
#include <libsdb/process.hpp>
#include <memory>

namespace sdb
{
    using opt_int     = std::optional<int>;
    using process_ptr = std::unique_ptr<process>;
    using elf_ptr     = std::unique_ptr<elf>;

    class target;
    using target_ptr = std::unique_ptr<target>;

    class target
    {

      public:
        target()                          = delete;
        target(const target &)            = delete;
        target &operator=(const target &) = delete;

        static target_ptr launch(std::filesystem::path path, opt_int stdout_replacement = std::nullopt);
        static target_ptr attach(pid_t pid);

        process &
        get_process()
        {
            return *process_;
        }

        const process &
        get_process() const
        {
            return *process_;
        }

        elf &
        get_elf()
        {
            return *elf_;
        }

        const elf &
        get_elf() const
        {
            return *elf_;
        }

      private:
        target(process_ptr proc, elf_ptr obj) : process_(std::move(proc)), elf_(std::move(obj))
        {
        }

        process_ptr process_;
        elf_ptr     elf_;
    };
} // namespace sdb
