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

    regs.write_by_id(sdb::register_id::mm0, 0xba5eba11);
    proc->resume();
    proc->wait_on_signal();
    output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "0xba5eba11");

    regs.write_by_id(sdb::register_id::xmm0, 42.24);
    proc->resume();
    proc->wait_on_signal();
    output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "42.24");

    regs.write_by_id(sdb::register_id::st0, 42.24l);
    regs.write_by_id(sdb::register_id::fsw, std::uint16_t{0b0011100000000000});
    regs.write_by_id(sdb::register_id::ftw, std::uint16_t{0b0011111111111111});
    proc->resume();
    proc->wait_on_signal();
    output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "42.24");
}

TEST_CASE("Read register works", "[register]")
{
    auto  proc = sdb::process::launch("targets/reg_read");
    auto &regs = proc->get_registers();

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint64_t>(sdb::register_id::r13) == 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint8_t>(sdb::register_id::r13b) == 42);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<sdb::byte64>(sdb::register_id::mm0) == sdb::to_byte64(0xba5eba11ull));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<sdb::byte128>(sdb::register_id::xmm0) == sdb::to_byte128(64.125));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<long double>(sdb::register_id::st0) == 64.125L);
}

TEST_CASE("Can create breakpoint site", "[breakpoint]")
{
    auto  proc = sdb::process::launch("targets/run_endlessly");
    auto &site = proc->create_breakpoint_site(sdb::virt_addr{42});
    REQUIRE(site.address().addr() == 42);
}

TEST_CASE("Breakpoint site ids increase", "[breakpoint]")
{
    auto  proc = sdb::process::launch("targets/run_endlessly");
    auto &s1   = proc->create_breakpoint_site(sdb::virt_addr{42});
    REQUIRE(s1.address().addr() == 42);

    auto &s2 = proc->create_breakpoint_site(sdb::virt_addr{43});
    REQUIRE(s2.id() == s1.id() + 1);
    auto &s3 = proc->create_breakpoint_site(sdb::virt_addr{44});
    REQUIRE(s3.id() == s1.id() + 2);
    auto &s4 = proc->create_breakpoint_site(sdb::virt_addr{45});
    REQUIRE(s4.id() == s1.id() + 3);
}

TEST_CASE("Can find breakpoint site", "[breakpoint]")
{
    auto        proc  = sdb::process::launch("targets/run_endlessly");
    const auto &cproc = proc;

    proc->create_breakpoint_site(sdb::virt_addr{42});
    proc->create_breakpoint_site(sdb::virt_addr{43});
    proc->create_breakpoint_site(sdb::virt_addr{44});
    proc->create_breakpoint_site(sdb::virt_addr{45});

    REQUIRE(proc->breakpoint_sites().contains_address(sdb::virt_addr{44}));

    auto &s1 = proc->breakpoint_sites().get_by_address(sdb::virt_addr{44});
    REQUIRE(s1.address().addr() == 44);

    auto &cs1 = cproc->breakpoint_sites().get_by_address(sdb::virt_addr{44});
    REQUIRE(cproc->breakpoint_sites().contains_address(sdb::virt_addr{44}));
    REQUIRE(cs1.address().addr() == 44);

    REQUIRE(proc->breakpoint_sites().contains_id(s1.id() + 1));

    auto &s2 = proc->breakpoint_sites().get_by_id(s1.id() + 1);
    REQUIRE(s2.id() == s1.id() + 1);
    REQUIRE(s2.address().addr() == 45);

    auto &cs2 = proc->breakpoint_sites().get_by_id(cs1.id() + 1);
    REQUIRE(cproc->breakpoint_sites().contains_id(cs1.id() + 1));
    REQUIRE(cs2.id() == cs1.id() + 1);
    REQUIRE(cs2.address().addr() == 45);
}

TEST_CASE("Cannot find breakpoint site", "[breakpoint]")
{
    auto        proc  = sdb::process::launch("targets/run_endlessly");
    const auto &cproc = proc;

    REQUIRE_THROWS_AS(proc->breakpoint_sites().get_by_address(sdb::virt_addr{44}), sdb::error);
    REQUIRE_THROWS_AS(proc->breakpoint_sites().get_by_id(44), sdb::error);

    REQUIRE_THROWS_AS(cproc->breakpoint_sites().get_by_address(sdb::virt_addr{44}), sdb::error);
    REQUIRE_THROWS_AS(cproc->breakpoint_sites().get_by_id(44), sdb::error);
}

TEST_CASE("Breakpoint site list size and emptiness", "[breakpoint]")
{
    auto        proc  = sdb::process::launch("targets/run_endlessly");
    const auto &cproc = proc;

    REQUIRE(proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 0);

    REQUIRE(cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 0);

    proc->create_breakpoint_site(sdb::virt_addr{42});

    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 1);

    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 1);

    proc->create_breakpoint_site(sdb::virt_addr{43});

    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 2);

    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 2);
}

TEST_CASE("Can iterate breakpoint sites", "[breakpoint]")
{
    auto        proc  = sdb::process::launch("targets/run_endlessly");
    const auto &cproc = proc;

    proc->create_breakpoint_site(sdb::virt_addr{42});
    proc->create_breakpoint_site(sdb::virt_addr{43});
    proc->create_breakpoint_site(sdb::virt_addr{44});
    proc->create_breakpoint_site(sdb::virt_addr{45});

    proc->breakpoint_sites().for_each([addr = 42](auto &site) mutable { REQUIRE(site.address().addr() == addr++); });
    cproc->breakpoint_sites().for_each([addr = 42](auto &site) mutable { REQUIRE(site.address().addr() == addr++); });
}
