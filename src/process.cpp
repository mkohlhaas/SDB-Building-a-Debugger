#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/process.hpp>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    void
    exit_with_perror(sdb::pipe &channel, std::string const &prefix)
    {
        auto message = prefix + ": " + std::strerror(errno);
        channel.write(reinterpret_cast<std::byte *>(message.data()), message.size());
        exit(-1);
    }
} // namespace

sdb::proc_ptr
sdb::process::launch(std::filesystem::path path, bool debug, std::optional<int> stdout_replacement)
{
    pipe  channel(true);
    pid_t pid;

    if ((pid = fork()) < 0)
    {
        error::send_errno("fork failed");
    }

    // we are inside the child process
    if (pid == 0)
    {
        personality(ADDR_NO_RANDOMIZE);
        channel.close_read(); // child process only writes

        if (stdout_replacement)
        {
            if (dup2(*stdout_replacement, STDOUT_FILENO) < 0)
            {
                exit_with_perror(channel, "stdout replacement failed");
            }
        }

        if (debug and ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0)
        {
            exit_with_perror(channel, "tracing failed");
        }

        // starts new code path for child
        if (execlp(path.c_str(), path.c_str(), nullptr) < 0)
        {
            exit_with_perror(channel, "exec failed");
        }
    }

    channel.close_write(); // parent, only reads, no writes

    // read from pipe
    auto data = channel.read();
    channel.close_read(); // done reading

    if (data.size() > 0)
    {
        waitpid(pid, nullptr, 0);
        auto chars = reinterpret_cast<char *>(data.data());
        error::send(std::string(chars, chars + data.size()));
    }

    sdb::proc_ptr proc(new process(pid, true, debug));
    if (debug)
    {
        proc->wait_on_signal();
    }

    return proc;
}

sdb::proc_ptr
sdb::process::attach(pid_t pid)
{
    if (pid == 0)
    {
        error::send("invalid PID");
    }

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
    {
        error::send_errno("could not attach");
    }

    proc_ptr proc(new process(pid, false, true));
    proc->wait_on_signal();

    return proc;
}

sdb::process::~process()
{
    if (pid_ != 0)
    {
        int status;

        if (is_attached_)
        {
            if (state_ == proc_state::running)
            {
                kill(pid_, SIGSTOP);
                waitpid(pid_, &status, 0);
            }
            ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
            kill(pid_, SIGCONT);
        }

        if (terminate_on_end_)
        {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

void
sdb::process::resume()
{
    auto pc = get_pc();
    if (breakpoint_sites_.enabled_stoppoint_at_address(pc))
    {
        auto &bp = breakpoint_sites_.get_by_address(pc);
        bp.disable();
        if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0)
        {
            error::send_errno("failed to single step");
        }
        int wait_status;
        if (waitpid(pid_, &wait_status, 0) < 0)
        {
            error::send_errno("waitpid failed");
        }
        bp.enable();
    }

    if (ptrace(PTRACE_CONT, pid_, nullptr, nullptr) < 0)
    {
        error::send_errno("could not resume");
    }

    state_ = proc_state::running;
}

sdb::stop_reason
sdb::process::wait_on_signal()
{
    int wait_status;
    int options = 0;

    if (waitpid(pid_, &wait_status, options) < 0)
    {
        error::send_errno("waitpid failed");
    }

    stop_reason reason(wait_status);
    state_ = reason.reason;

    if (is_attached_ and state_ == proc_state::stopped)
    {
        read_all_registers();

        auto instr_begin = get_pc() - 1;
        if (reason.info == SIGTRAP and breakpoint_sites_.enabled_stoppoint_at_address(instr_begin))
        {
            set_pc(instr_begin);
        }
    }

    return reason;
}

sdb::stop_reason::stop_reason(int wait_status)
{
    if (WIFEXITED(wait_status))
    {
        reason = proc_state::exited;
        info   = WEXITSTATUS(wait_status);
    }
    else if (WIFSIGNALED(wait_status))
    {
        reason = proc_state::terminated;
        info   = WTERMSIG(wait_status);
    }
    else if (WIFSTOPPED(wait_status))
    {
        reason = proc_state::stopped;
        info   = WSTOPSIG(wait_status);
    }
}

// read registers into user struct
void
sdb::process::read_all_registers()
{
    if (ptrace(PTRACE_GETREGS, pid_, nullptr, &get_registers().data_.regs) < 0)
    {
        error::send_errno("could not read GPR registers");
    }

    if (ptrace(PTRACE_GETFPREGS, pid_, nullptr, &get_registers().data_.i387) < 0)
    {
        error::send_errno("could not read FPR registers");
    }

    for (int i = 0; i < 8; ++i)
    {
        // Read debug registers
        for (int i = 0; i < 8; ++i)
        {
            auto id   = static_cast<int>(register_id::dr0) + i;
            auto info = register_info_by_id(static_cast<register_id>(id));

            errno = 0;

            std::int64_t data = ptrace(PTRACE_PEEKUSER, pid_, info.offset, nullptr);

            if (errno != 0)
            {
                error::send_errno("could not read debug register");
            }

            get_registers().data_.u_debugreg[i] = data;
        }
    }
}

// write user struct (data) into user area
void
sdb::process::write_user_area(std::size_t offset, std::uint64_t data)
{
    if (ptrace(PTRACE_POKEUSER, pid_, offset, data) < 0)
    {
        error::send_errno("could not write to user area");
    }
}

void
sdb::process::write_fprs(const user_fpregs_struct &fprs)
{
    if (ptrace(PTRACE_SETFPREGS, pid_, nullptr, &fprs) < 0)
    {
        error::send_errno("could not write floating point registers");
    }
}

void
sdb::process::write_gprs(const user_regs_struct &gprs)
{
    if (ptrace(PTRACE_SETREGS, pid_, nullptr, &gprs) < 0)
    {
        error::send_errno("could not write general purpose registers");
    }
}

sdb::breakpoint_site &
sdb::process::create_breakpoint_site(virt_addr address)
{
    if (breakpoint_sites_.contains_address(address))
    {
        error::send("breakpoint site already created at address " + std::to_string(address.addr()));
    }
    return breakpoint_sites_.push(std::unique_ptr<breakpoint_site>(new breakpoint_site(*this, address)));
}

sdb::stop_reason
sdb::process::step_instruction()
{
    std::optional<breakpoint_site *> to_reenable;

    auto pc = get_pc();
    if (breakpoint_sites_.enabled_stoppoint_at_address(pc))
    {
        auto &bp = breakpoint_sites_.get_by_address(pc);
        bp.disable();
        to_reenable = &bp;
    }

    if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0)
    {
        error::send_errno("could not single step");
    }

    auto reason = wait_on_signal();

    if (to_reenable)
    {
        to_reenable.value()->enable();
    }

    return reason;
}
