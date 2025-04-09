#pragma once

#include <cstdint>
#include <filesystem>
#include <libsdb/breakpoint_site.hpp>
#include <libsdb/registers.hpp>
#include <libsdb/stoppoint_collection.hpp>
#include <memory>
#include <optional>
#include <sys/types.h>

namespace sdb
{

    enum class proc_state
    {
        stopped,
        running,
        exited,
        terminated
    };

    struct stop_reason
    {
        stop_reason(int wait_status);

        proc_state   reason;
        std::uint8_t info;
    };

    using proc_ptr = std::unique_ptr<process>;

    class process
    {
        process()                           = delete;
        process(const process &)            = delete;
        process &operator=(const process &) = delete;

      public:
        ~process();

        static proc_ptr launch(std::filesystem::path path, bool debug = true,
                               std::optional<int> stdout_replacement = std::nullopt);
        static proc_ptr attach(pid_t pid);
        void            resume();

        stop_reason wait_on_signal();

        proc_state
        state() const
        {
            return state_;
        }

        pid_t
        pid() const
        {
            return pid_;
        }

        registers &
        get_registers()
        {
            return *registers_;
        }

        const registers &
        get_registers() const
        {
            return *registers_;
        }

        void write_user_area(std::size_t offset, std::uint64_t data);

        void write_fprs(const user_fpregs_struct &fprs);
        void write_gprs(const user_regs_struct &gprs);

        virt_addr
        get_pc() const
        {
            return virt_addr{get_registers().read_by_id_as<std::uint64_t>(register_id::rip)};
        }

        breakpoint_site &create_breakpoint_site(virt_addr address);
        // Some way to iterate over and remove breakpoint sites

        stoppoint_collection<breakpoint_site> &
        breakpoint_sites()
        {
            return breakpoint_sites_;
        }

        const stoppoint_collection<breakpoint_site> &
        breakpoint_sites() const
        {
            return breakpoint_sites_;
        }

        void
        set_pc(virt_addr address)
        {
            get_registers().write_by_id(register_id::rip, address.addr());
        }

        sdb::stop_reason step_instruction();

      private:
        process(pid_t pid, bool terminate_on_end, bool is_attached)
            : pid_(pid), terminate_on_end_(terminate_on_end), is_attached_(is_attached),
              registers_(new registers(*this))
        {
        }

        void read_all_registers();

        using regs_ptr = std::unique_ptr<registers>;
        using bp_sites = stoppoint_collection<breakpoint_site>;

        pid_t      pid_{0};
        bool       terminate_on_end_{true};
        bool       is_attached_{true};
        proc_state state_{proc_state::stopped};
        regs_ptr   registers_;
        bp_sites   breakpoint_sites_;
    };
} // namespace sdb
