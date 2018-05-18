#include "Blabla.hpp"

#include <thread>
#include <unordered_set>

#include <boost/thread/shared_lock_guard.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>

#include <commonpp/core/LoggingInterface.hpp>
#include <commonpp/thread/Thread.hpp>

#include "Router.hpp"

#include "handlers/Acceptor.hpp"
#include "handlers/Client.hpp"

namespace blabla
{

CREATE_LOGGER(log, "service");

namespace detail
{
struct Service : handlers::ClientManager
{
    Service(commonpp::thread::ThreadPool& pool, const ServiceConfiguration& conf)
    : pool(pool)
    , conf(conf)
    {
        start();
    }

    ~Service()
    {
        stop();
    }

    void start()
    {
        start_acceptor();
    }

    void stop()
    {
        stop_acceptor();
        stop_connections();
    }

    void start_acceptor()
    {
        DLOG(log, info) << "Starting acceptors";
        for (auto& address : conf.service.addresses)
        {
            auto addr = boost::asio::ip::address::from_string(address.address);
            acceptors.emplace_back(std::make_unique<handlers::Acceptor>(
                pool, std::move(addr), address.port));

            acceptors.back()->start<handlers::Client>(
                std::bind(&Service::on_new_client, this, std::placeholders::_1));

            LOG(log, info) << "Started listening: " << addr.to_string()
                           << " on port: " << address.port;
        }
    }

    void stop_acceptor()
    {
        acceptors.clear();
    }

    void stop_connections()
    {
        DLOG(log, debug) << "Stopping connection...";
        while (true)
        {
            {
                {
                    boost::unique_lock<boost::shared_mutex> l(mutex);
                    if (conns.empty())
                    {
                        DLOG(log, debug) << "... all connections stopped";
                        return;
                    }

                    auto conns = this->conns;
                    l.unlock(); // avoid deadlock during notification.
                    for (auto conn : conns)
                    {
                        DLOG(log, debug)
                            << "...stopping connection: " << conn->peer();
                        conn->stop();
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::yield();
        }
    }

    void on_new_client(std::shared_ptr<handlers::Client> client) override
    {
        DLOG(log, debug) << "Got a new connection from: " << client->peer();

        auto cl = client.get();
        {
            boost::unique_lock<boost::shared_mutex> l(mutex);
            conns.insert(std::move(client));
        }
        cl->start(this);
    }

    void remove_connection(std::shared_ptr<handlers::Client> client) override
    {
        boost::unique_lock<boost::shared_mutex> l(mutex);
        if (conns.erase(client) != 1)
        {
            LOG(log, error)
                << "Could not find a client to erase: " << client->peer()
                << ", this shows that an inconsistency happened";
        }
    }

    std::vector<handlers::SubscriptionNode*> subscribe(
        std::vector<handlers::Subscription> subs, handlers::Client* client) override
    {
        return router.add(std::move(subs), *client);
    }

    std::vector<handlers::SubscriptionNode*> unsubscribe(
        std::vector<boost::string_view> subs, handlers::Client* client) override
    {
        return router.remove(std::move(subs), *client);
    }

    void emit_to(std::string route,
                 std::unique_ptr<handlers::SharedBufferWithSpecificMetadata> msg) override
    {

        services::blabla::ConsumerMessageHeader header;
        {
            header.mutable_header()->set_type(services::blabla::MESSAGE);
            header.set_route(route);
            header.set_message_size(msg->payload_size());
        }

        // Lock to avoid a client being destroyed while one is trying to deliver
        // a message.
        auto emit_lambda = [msg = std::move(msg), route, header](
                               handlers::Client& cl, int32_t correlation_id) {
            auto current_client_metadata = header;
            {
                current_client_metadata.set_correlation_id(correlation_id);
            }
            std::vector<uint8_t> metadata;
            {
                metadata.resize(current_client_metadata.ByteSize());
                current_client_metadata.SerializeToArray(metadata.data(),
                                                         metadata.size());
            }
            cl.send(msg->new_with_metadata(std::move(metadata)));
        };

        boost::shared_lock<boost::shared_mutex> l(mutex);
        for (auto& sub : router.subscriptions_for(route))
        {
            sub->foreach_client(emit_lambda);
        }
    }

    commonpp::thread::ThreadPool& pool;
    const ServiceConfiguration& conf;

    std::vector<std::unique_ptr<handlers::Acceptor>> acceptors;
    mutable boost::shared_mutex mutex;
    std::unordered_set<std::shared_ptr<handlers::Client>> conns;
    Router router;
};
} // namespace detail

Service::Service(ServiceConfiguration _conf)
: conf(std::move(_conf))
, pool(conf.threads.io_threads, "blabla", conf.threads.io_context)
{
}

Service::~Service()
{
    service.reset();
    pool.stop();
}

void Service::start()
{
    using commonpp::thread::ThreadPool;
    using namespace commonpp::thread;
    pool.start([] {}, ThreadPool::ThreadDispatchPolicy::DispatchToPCore);

    service = std::make_unique<detail::Service>(pool, conf);
}

} // namespace blabla
