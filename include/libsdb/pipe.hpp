#pragma once

#include <cstddef>
#include <vector>

namespace sdb
{
    using vec_bytes = std::vector<std::byte>;

    class pipe
    {
      public:
        explicit pipe(bool close_on_exec);

        ~pipe();

        int
        get_read() const
        {
            return fds_[read_fd];
        }

        int
        get_write() const
        {
            return fds_[write_fd];
        }

        int  release_read();
        int  release_write();
        void close_read();
        void close_write();

        vec_bytes read();
        void      write(std::byte *from, std::size_t bytes);

      private:
        static constexpr unsigned read_fd{0};
        static constexpr unsigned write_fd{1};

        int fds_[2];
    };
} // namespace sdb
