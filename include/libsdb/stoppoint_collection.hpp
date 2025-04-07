#pragma once

#include <algorithm>
#include <libsdb/error.hpp>
#include <libsdb/types.hpp>
#include <memory>
#include <vector>

namespace sdb
{
    template <typename Stoppoint>
    class stoppoint_collection
    {
      public:
        Stoppoint &push(std::unique_ptr<Stoppoint> bs);

        bool contains_id(Stoppoint::id_type id) const;
        bool contains_address(virt_addr address) const;
        bool enabled_stoppoint_at_address(virt_addr address) const;

        Stoppoint &get_by_id(Stoppoint::id_type id);
        Stoppoint &get_by_address(virt_addr address);

        const Stoppoint &get_by_id(Stoppoint::id_type id) const;
        const Stoppoint &get_by_address(virt_addr address) const;

        void remove_by_id(Stoppoint::id_type id);
        void remove_by_address(virt_addr address);

        template <typename F>
        void for_each(F f);

        template <typename F>
        void for_each(F f) const;

        std::size_t
        size() const
        {
            return stoppoints_.size();
        }

        bool
        empty() const
        {
            return stoppoints_.empty();
        }

      private:
        using points_t = std::vector<std::unique_ptr<Stoppoint>>;
        points_t stoppoints_;

        points_t::iterator       find_by_id(Stoppoint::id_type id);
        points_t::const_iterator find_by_id(Stoppoint::id_type id) const;
        points_t::iterator       find_by_address(virt_addr address);
        points_t::const_iterator find_by_address(virt_addr address) const;
    };

    template <typename Stoppoint>
    Stoppoint &
    stoppoint_collection<Stoppoint>::push(std::unique_ptr<Stoppoint> bs)
    {
        stoppoints_.push_back(std::move(bs));
        return *stoppoints_.back();
    }

    template <typename Stoppoint>
    auto
    stoppoint_collection<Stoppoint>::find_by_id(Stoppoint::id_type id) -> points_t::iterator
    {
        return std::find_if(begin(stoppoints_), std::end(stoppoints_), [=](auto &point) { return point->id() == id; });
    }

    template <typename Stoppoint>
    auto
    stoppoint_collection<Stoppoint>::find_by_id(Stoppoint::id_type id) const -> points_t::const_iterator
    {
        return const_cast<stoppoint_collection *>(this)->find_by_id(id);
    }

    template <typename Stoppoint>
    auto
    stoppoint_collection<Stoppoint>::find_by_address(virt_addr address) -> points_t::iterator
    {
        return std::find_if(begin(stoppoints_), std::end(stoppoints_),
                            [=](auto &point) { return point->at_address(address); });
    }

    template <typename Stoppoint>
    auto
    stoppoint_collection<Stoppoint>::find_by_address(virt_addr address) const -> points_t::const_iterator
    {
        return const_cast<stoppoint_collection *>(this)->find_by_address(address);
    }

    template <typename Stoppoint>
    bool
    stoppoint_collection<Stoppoint>::contains_id(typename Stoppoint::id_type id) const
    {
        return find_by_id(id) != std::end(stoppoints_);
    }

    template <typename Stoppoint>
    bool
    stoppoint_collection<Stoppoint>::contains_address(virt_addr address) const
    {
        return find_by_address(address) != std::end(stoppoints_);
    }

    template <typename Stoppoint>
    bool
    stoppoint_collection<Stoppoint>::enabled_stoppoint_at_address(virt_addr address) const
    {
        return contains_address(address) and get_by_address(address).is_enabled();
    }

    template <typename Stoppoint>
    Stoppoint &
    stoppoint_collection<Stoppoint>::get_by_id(Stoppoint::id_type id)
    {
        auto it = find_by_id(id);
        if (it == std::end(stoppoints_))
        {
            error::send("invalid stoppoint id");
        }
        return **it;
    }

    template <typename Stoppoint>
    const Stoppoint &
    stoppoint_collection<Stoppoint>::get_by_id(Stoppoint::id_type id) const
    {
        return const_cast<stoppoint_collection *>(this)->get_by_id(id);
    }

    template <class Stoppoint>
    Stoppoint &
    stoppoint_collection<Stoppoint>::get_by_address(virt_addr address)
    {
        auto it = find_by_address(address);
        if (it == std::end(stoppoints_))
        {
            error::send("stoppoint with given address not found");
        }
        return **it;
    }

    template <class Stoppoint>
    const Stoppoint &
    stoppoint_collection<Stoppoint>::get_by_address(virt_addr address) const
    {
        return const_cast<stoppoint_collection *>(this)->get_by_address(address);
    }

    template <class Stoppoint>
    void
    stoppoint_collection<Stoppoint>::remove_by_id(Stoppoint::id_type id)
    {
        auto it = find_by_id(id);
        (**it).disable();
        stoppoints_.erase(it);
    }

    template <class Stoppoint>
    void
    stoppoint_collection<Stoppoint>::remove_by_address(virt_addr address)
    {
        auto it = find_by_address(address);
        (**it).disable();
        stoppoints_.erase(it);
    }

    // two template lines bc member function template of a class template
    template <class Stoppoint>
    template <typename F>
    void
    stoppoint_collection<Stoppoint>::for_each(F f)
    {
        for (auto &point : stoppoints_)
        {
            f(*point);
        }
    }

    template <class Stoppoint>
    template <typename F>
    void
    stoppoint_collection<Stoppoint>::for_each(F f) const
    {
        for (const auto &point : stoppoints_)
        {
            f(*point);
        }
    }
} // namespace sdb
