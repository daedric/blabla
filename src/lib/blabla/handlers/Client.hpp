#pragma once

#include <memory>
#include <mutex>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/container/flat_map.hpp>
#include <tbb/spin_rw_mutex.h>

#include <commonpp/core/LoggingInterface.hpp>
#include <commonpp/thread/ThreadPool.hpp>

#include "Buffer.hpp"
#include "Protocol.hpp"

namespace blabla
{
namespace handlers
{
struct Client;
struct SubscriptionNode;

struct ClientError
{
    boost::system::error_code errc;
};

using Route = boost::string_view;
using Subscription = std::pair<boost::string_view, int32_t>;

struct ClientManager
{
    virtual ~ClientManager() = default;

    virtual void on_new_client(std::shared_ptr<Client> client) = 0;
    virtual void remove_connection(std::shared_ptr<Client> client) = 0;

    virtual std::vector<SubscriptionNode*> subscribe(std::vector<Subscription>,
                                                     Client* client) = 0;
    virtual std::vector<SubscriptionNode*>
    unsubscribe(std::vector<boost::string_view>, Client* client) = 0;
    virtual void emit_to(std::string route,
                         std::unique_ptr<SharedBufferWithSpecificMetadata>) = 0;
};

// XXX: Maybe add a queue for round robin delivery amongst consumers.
struct SubscriptionNode
{
    void add_client(handlers::Client& client, int32_t correlation_id)
    {
        std::lock_guard<tbb::spin_rw_mutex> l(mutex);
        clients.emplace(std::addressof(client), correlation_id);
    }

    void remove_client(handlers::Client& client)
    {
        std::lock_guard<tbb::spin_rw_mutex> l(mutex);
        clients.erase(std::addressof(client));
    }

    template <typename CB>
    void foreach_client(CB&& cb)
    {
        mutex.lock_read();
        try
        {
            for (auto& pair : clients)
            {
                cb(*pair.first, pair.second);
            }
        }
        catch (...)
        {
            mutex.unlock();
            throw;
        }

        mutex.unlock();
    }

private:
    tbb::spin_rw_mutex mutex;
    boost::container::flat_map<handlers::Client*, int32_t> clients;
};

struct Client : MessageCracker<Client>, std::enable_shared_from_this<Client>
{
    using tcp = boost::asio::ip::tcp;
    friend MessageCracker<Client>;

    struct DispatchContext
    {
        std::shared_ptr<Client> myself;
    };

private:
    Client(commonpp::thread::ThreadPool& pool)
    : pool(pool)
    , socket_(pool.getService())
    {
    }

public:
    static std::shared_ptr<Client> create(commonpp::thread::ThreadPool& pool)
    {
        return std::shared_ptr<Client>(new Client(pool));
    }

    auto peer() const
    {
        boost::system::error_code ec;
        const auto& e = socket_.remote_endpoint(ec);
        if (BOOST_LIKELY(!ec))
        {
            return "[" + e.address().to_string() + ":" +
                   std::to_string(e.port()) + "]";
        }
        return std::string("[connection lost]");
    }

    ~Client() = default;

    void start(ClientManager* manager);
    void stop();

    // unsafe, should not be used by the client code..
    boost::asio::ip::tcp::socket& socket() noexcept
    {
        return socket_;
    }
    const boost::asio::ip::tcp::socket& socket() const noexcept
    {
        return socket_;
    }

    void send(SharedBuffer::SharedBufferPtr);
    void send(std::unique_ptr<SharedBufferWithSpecificMetadata>);

private:
    template <typename T>
    void send_impl(T buffer);

    template <typename T>
    void send_error(T buffer);

    void killme();
    bool handle_error(boost::system::error_code);

    void read_message(std::shared_ptr<Client>);
    void maybe_read_message_size(std::shared_ptr<Client>,
                                 boost::system::error_code,
                                 std::size_t);
    void maybe_read_message(std::shared_ptr<Client>,
                            boost::system::error_code,
                            std::size_t);
    void maybe_read_payload(std::shared_ptr<Client>,
                            std::string,
                            boost::system::error_code,
                            std::size_t);
    void unsubscribe_all();

private:
    void decoding_error();
    void decoding_unknown_type();

    template <typename T>
    void handle(DispatchContext&, T&);

private:
    mutable std::mutex mutex;
    commonpp::thread::ThreadPool& pool;
    tcp::socket socket_;

    // XXX: put this definition somewhere
    IntBuffer size_buffer;
    ClientManager* manager = nullptr;
    std::vector<uint8_t> control_message_buffer;
    std::vector<uint8_t> raw_payload_buffer;
    // XXX: micro race condition if we stop the server while we process a
    // subscription request.
    std::vector<SubscriptionNode*> active_subscriptions;
};

inline auto& socket(std::shared_ptr<Client>& client)
{
    assert(client != nullptr);
    return client->socket();
}

inline auto& socket(Client* client)
{
    assert(client != nullptr);
    return client->socket();
}
inline auto& socket(Client& client)
{
    return client.socket();
}

} // namespace handlers
} // namespace blabla
