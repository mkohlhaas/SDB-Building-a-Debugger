#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <libsdb/bit.hpp>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/process.hpp>
#include <signal.h>
#include <sys/types.h>

namespace
{
    bool
    process_exists(pid_t pid)
    {
        auto ret = kill(pid, 0);
        return ret != -1 and errno != ESRCH;
    }

    char
    get_process_status(pid_t pid)
    {
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
        std::string   data;
        std::getline(stat, data);

        auto index_of_last_parenthesis = data.rfind(')');
        auto index_of_status_indicator = index_of_last_parenthesis + 2;
        return data[index_of_status_indicator];
    }
} // namespace

TEST_CASE("process::launch success", "[process]")
{
    auto proc = sdb::process::launch("yes");
    REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("process::launch no such program", "[process]")
{
    REQUIRE_THROWS_AS(sdb::process::launch("you_do_not_have_to_be_good"), sdb::error);
}

TEST_CASE("process::attach success", "[process]")
{
    auto target = sdb::process::launch("targets/run_endlessly", false);
    auto proc   = sdb::process::attach(target->pid());
    REQUIRE(get_process_status(target->pid()) == 't');
}

TEST_CASE("process::attach invalid PID", "[process]")
{
    REQUIRE_THROWS_AS(sdb::process::attach(0), sdb::error);
}

TEST_CASE("process::resume success", "[process]")
{
    {
        auto proc = sdb::process::launch("targets/run_endlessly");
        proc->resume();
        auto status  = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }

    {
        auto target = sdb::process::launch("targets/run_endlessly", false);
        auto proc   = sdb::process::attach(target->pid());
        proc->resume();
        auto status  = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
}

TEST_CASE("process::resume already terminated", "[process]")
{
    auto proc = sdb::process::launch("targets/end_immediately");
    proc->resume();
    proc->wait_on_signal();
    REQUIRE_THROWS_AS(proc->resume(), sdb::error);
}

TEST_CASE("Write register works", "[register]")
{
    bool      close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = sdb::process::launch("targets/reg_write", true, channel.get_write());
    channel.close_write();
    proc->resume();
    proc->wait_on_signal();
    auto &regs = proc->get_registers();
    regs.write_by_id(sdb::register_id::rsi, 0xcafecafe);
    proc->resume();
    proc->wait_on_signal();
    auto output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "0xcafecafe");
}
