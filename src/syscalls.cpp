#include <libsdb/error.hpp>
#include <libsdb/syscalls.hpp>
#include <unordered_map>

namespace
{
    using map_name_syscallid = std::unordered_map<std::string_view, int>;

    const map_name_syscallid g_syscall_name_map = {
#define DEFINE_SYSCALL(name, id) {#name, id},
#include "include/syscalls.inc"
#undef DEFINE_SYSCALL
    };
} // namespace

int
sdb::syscall_name_to_id(std::string_view name)
{
    if (g_syscall_name_map.count(name) != 1)
    {
        sdb::error::send("no such syscall");
    }
    return g_syscall_name_map.at(name);
}

std::string_view
sdb::syscall_id_to_name(int id)
{
    switch (id)
    {
#define DEFINE_SYSCALL(name, id)                                                                                       \
    case id:                                                                                                           \
        return #name;
#include "include/syscalls.inc"
#undef DEFINE_SYSCALL
    default:
        sdb::error::send("no such syscall");
    }
}
