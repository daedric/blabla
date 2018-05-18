#pragma once

#include <deque>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <boost/container/flat_set.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/utility/string_view.hpp>
#include <murmur3.h>
#include <tsl/htrie_map.h>

#include "handlers/Client.hpp"

namespace blabla
{
namespace detail
{
struct StrHash
{
    std::size_t operator()(const char* key, std::size_t key_size) const
    {
        struct hash
        {
            uint64_t high;
            uint64_t low;
        };

        hash h;
        MurmurHash3_x64_128(key, key_size, 0, &h);
        return h.low * (key_size * h.high);
    }
};
} // namespace detail

struct Router
{
    ~Router();

    std::vector<handlers::SubscriptionNode*>
    add(std::vector<handlers::Subscription> routes, handlers::Client& client);

    std::vector<handlers::SubscriptionNode*>
    remove(std::vector<boost::string_view> routes, handlers::Client& client);

    std::vector<handlers::SubscriptionNode*>
    subscriptions_for(boost::string_view route);

private:
    // The container has been chosen for memory usage while having pretty decent
    // performances. For performances improvement, there might be some other
    // containers we could try such as the ones presented in the following
    // paper: https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html
    boost::shared_mutex mutex;
    tsl::htrie_map<char, handlers::SubscriptionNode*, detail::StrHash> routes;
};

} // namespace blabla
