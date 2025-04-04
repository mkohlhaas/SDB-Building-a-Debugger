#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
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

    class process;
    using proc_ptr = std::unique_ptr<process>;

    class process
    {
        process()                           = delete;
        process(const process &)            = delete;
        process &operator=(const process &) = delete;

      public:
        ~process();

        static proc_ptr launch(std::filesystem::path path, bool debug = true);
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

      private:
        process(pid_t pid, bool terminate_on_end, bool is_attached)
            : pid_(pid), terminate_on_end_(terminate_on_end), is_attached_(is_attached)
        {
        }

        pid_t      pid_{0};
        bool       terminate_on_end_{true};
        bool       is_attached_{true};
        proc_state state_{proc_state::stopped};
    };
} // namespace sdb
