#include <libsdb/target.hpp>
#include <libsdb/types.hpp>

namespace
{
    sdb::elf_ptr
    create_loaded_elf(const sdb::process &proc, const std::filesystem::path &path)
    {
        auto auxv = proc.get_auxv();
        auto obj  = std::make_unique<sdb::elf>(path);
        obj->notify_loaded(sdb::virt_addr(auxv[AT_ENTRY] - obj->get_header().e_entry));
        return obj;
    }
} // namespace

sdb::target_ptr
sdb::target::launch(std::filesystem::path path, opt_int stdout_replacement)
{
    auto proc = process::launch(path, true, stdout_replacement);
    auto obj  = create_loaded_elf(*proc, path);

    return target_ptr(new target(std::move(proc), std::move(obj)));
}

sdb::target_ptr
sdb::target::attach(pid_t pid)
{
    auto elf_path = std::filesystem::path("/proc") / std::to_string(pid) / "exe";
    auto proc     = process::attach(pid);
    auto obj      = create_loaded_elf(*proc, elf_path);

    return target_ptr(new target(std::move(proc), std::move(obj)));
}
