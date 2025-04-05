#pragma once

#include <libsdb/register_info.hpp>
#include <libsdb/types.hpp>
#include <sys/user.h>
#include <variant>

namespace sdb
{
    class process;

    class registers
    {
      public:
        registers()                             = delete;
        registers(const registers &)            = delete;
        registers &operator=(const registers &) = delete;

        using value = std::variant<std::uint8_t,  //
                                   std::uint16_t, //
                                   std::uint32_t, //
                                   std::uint64_t, //
                                   std::int8_t,   //
                                   std::int16_t,  //
                                   std::int32_t,  //
                                   std::int64_t,  //
                                   float,         //
                                   double,        //
                                   long double,   //
                                   byte64,        //
                                   byte128>;      //

        value read(const register_info &info) const;
        void  write(const register_info &info, value val);

        template <typename T>
        T
        read_by_id_as(register_id id) const
        {
            return std::get<T>(read(register_info_by_id(id)));
        }

        void
        write_by_id(register_id id, value val)
        {
            write(register_info_by_id(id), val);
        }

      private:
        friend process; // only sdb::process should be able to construct an sdb::registers object

        registers(process &proc) : proc_(&proc)
        {
        }

        user     data_; // parent process (proc_) will read all the registers into
        process *proc_; // data_ when the inferior is halted
    };
} // namespace sdb
