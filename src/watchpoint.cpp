#include <libsdb/error.hpp>
#include <libsdb/process.hpp>
#include <libsdb/watchpoint.hpp>
#include <utility>

namespace
{
    auto
    get_next_id()
    {
        static sdb::watchpoint::id_type id = 0;
        return ++id;
    }
} // namespace

sdb::watchpoint::watchpoint(process &proc, virt_addr address, stoppoint_mode mode, std::size_t size)
    : process_{&proc}, address_{address}, is_enabled_{false}, mode_{mode}, size_{size}
{
    // size should be 8, 4, 2, 1
    if ((address.addr() & (size - 1)) != 0)
    {
        error::send("watchpoint must be aligned to size");
    }

    id_ = get_next_id();
    update_data();
}

void
sdb::watchpoint::enable()
{
    if (is_enabled_)
    {
        return;
    }

    hardware_register_index_ = process_->set_watchpoint(id_, address_, mode_, size_);
    is_enabled_              = true;
}

void
sdb::watchpoint::disable()
{
    if (!is_enabled_)
    {
        return;
    }

    process_->clear_hardware_stoppoint(hardware_register_index_);
    is_enabled_ = false;
}

void
sdb::watchpoint::update_data()
{
    using vec_bytes = std::vector<std::byte>;

    std::uint64_t new_data = 0;
    vec_bytes     read     = process_->read_memory(address_, size_);

    memcpy(&new_data, read.data(), size_);

    previous_data_ = std::exchange(data_, new_data);
}
