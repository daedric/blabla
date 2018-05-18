#include "Router.hpp"

namespace blabla
{

Router::~Router()
{
    for (auto it = routes.begin(), end = routes.end(); it != end; ++it)
    {
        delete it.value();
    }

    routes.clear();
}

std::vector<handlers::SubscriptionNode*> Router::add(
    std::vector<handlers::Subscription> routes_to_add, handlers::Client& client)
{
    std::vector<handlers::SubscriptionNode*> subscriptions;
    subscriptions.reserve(routes_to_add.size());

    // only one can succeed but rlock are not blocked yet.
    boost::upgrade_lock<boost::shared_mutex> lock(mutex);
    for (auto& pair : routes_to_add)
    {
        auto& ref = pair.first;
        auto correlation_id = pair.second;

        auto it = routes.find_ks(ref.data(), ref.size());
        if (it != routes.end())
        {
            auto subscription = it.value();
            subscription->add_client(client, correlation_id);
            subscriptions.emplace_back(subscription);
            continue;
        }

        auto subscription = std::make_unique<handlers::SubscriptionNode>();
        subscription->add_client(client, correlation_id);
        {
            boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(lock);
            routes.insert_ks(ref.data(), ref.size(), subscription.get());
        }

        subscriptions.emplace_back(subscription.release());
    }

    return subscriptions;
}

std::vector<handlers::SubscriptionNode*>
Router::subscriptions_for(boost::string_view route)
{
    std::vector<handlers::SubscriptionNode*> subscriptions;
    boost::string_view subject_part;
    size_t idx = 0;

    boost::shared_lock<boost::shared_mutex> lock(mutex);
    while (subject_part != route)
    {
        idx = route.find_first_of('.', idx);
        if (idx != boost::string_view::npos)
        {
            subject_part = route.substr(0, idx);
            ++idx;
        }
        else
        {
            subject_part = route;
        }

        auto it = routes.find_ks(subject_part.data(), subject_part.size());
        if (it != routes.end())
        {
            subscriptions.emplace_back(it.value());
        }
    }

    return subscriptions;
}

std::vector<handlers::SubscriptionNode*> Router::remove(
    std::vector<boost::string_view> routes_to_rm, handlers::Client& client)
{
    std::vector<handlers::SubscriptionNode*> subscriptions;
    boost::shared_lock<boost::shared_mutex> lock(mutex);

    for (auto& ref : routes_to_rm)
    {
        auto it = routes.find_ks(ref.data(), ref.size());
        if (it != routes.end())
        {
            auto subscription = it.value();
            subscriptions.emplace_back(subscription);
            continue;
        }
    }

    return subscriptions;
}

} // namespace blabla