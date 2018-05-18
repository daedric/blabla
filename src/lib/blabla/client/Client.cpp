#include "Client.hpp"

#include <boost/asio.hpp>

#include <commonpp/core/LoggingInterface.hpp>
#include <commonpp/core/Utils.hpp>

namespace blabla
{
namespace client
{

CREATE_LOGGER(log, "blabla::client");

namespace detail
{
}

Client::Client(EventHandler& h)
: hndl_(h)
, pool_(std::make_shared<commonpp::thread::ThreadPool>(1, "blabla_client"))
, socket_(pool_->getService())
{
}

Client::Client(EventHandler& h, commonpp::thread::ThreadPool& pool)
: hndl_(h)
, pool_(std::addressof(pool), commonpp::get_fake_delete(std::addressof(pool)))
, socket_(pool_->getService())
{
}

Client::~Client()
{
}

using boost::asio::ip::tcp;

void Client::configure(BlablaClientConfiguration conf)
{
    DLOG(log, info) << "Configuring client";
    conf_ = std::move(conf);
    pool_->start();

    if (conf.sync_connect)
    {
        sync_connect();
    }
    else
    {
        async_connect();
    }
}

void Client::sync_connect()
{
    tcp::resolver resolver(pool_->getService());
    tcp::resolver::query query(conf_.host, std::to_string(conf_.port));

    DLOG(log, debug) << "Resolving: " << query.host_name() << ":"
                     << query.service_name();

    auto it = resolver.resolve(query);
    tcp::resolver::results_type end;

    std::lock_guard<std::mutex> l(sock_mutex_);
    boost::system::error_code ec;
    for (; it != end; ++it)
    {
        auto& endpoint = *it;
        LOG(log, info) << "Trying to connect to: "
                       << endpoint.endpoint().address() << ":"
                       << endpoint.endpoint().port();
        socket_.connect(*it, ec);

        if (ec)
        {
            LOG(log, warning)
                << "Could not connect to: " << endpoint.endpoint().address()
                << ":" << endpoint.endpoint().port() << ": " << ec.message();
        }
        else
        {
            LOG(log, info) << "Connected to: " << endpoint.endpoint().address()
                           << ":" << endpoint.endpoint().port();
            break;
        }
    }

    if (ec)
    {
        boost::asio::detail::throw_error(ec);
    }
}

void Client::async_connect()
{
    DLOG(log, debug) << "Resolving: " << conf_.host << ":" << conf_.port;
    auto resolver_ptr = std::make_unique<tcp::resolver>(pool_->getService());
    auto& resolver = *resolver_ptr;
    resolver.async_resolve(conf_.host, std::to_string(conf_.port),
                           [this, _ = std::move(resolver_ptr)](
                               const boost::system::error_code& ec, auto results) {
                               async_resolve(std::move(ec), std::move(results));
                           });
}

void Client::async_resolve(boost::system::error_code ec,
                           boost::asio::ip::tcp::resolver::results_type results)
{
    if (ec)
    {
        LOG(log, warning) << "Resolve error: " << ec.message() << ", retrying in "
                          << conf_.connect_retry_interval.count() << "ms";
        this->hndl_.connect_error(ec);
        pool_->schedule(conf_.connect_retry_interval, [this] {
            async_connect();
            return false;
        });
        return;
    }

    async_connect(std::move(results));
}

void Client::async_connect(boost::asio::ip::tcp::resolver::results_type results)
{
    if (results.empty())
    {
        LOG(log, warning)
            << "Connection error, not more host to try, retrying in "
            << conf_.connect_retry_interval.count() << "ms";
        this->hndl_.connect_error(boost::asio::error::not_found);
        pool_->schedule(conf_.connect_retry_interval, [this] {
            async_connect();
            return false;
        });
    }

    std::lock_guard<std::mutex> l(sock_mutex_);
    boost::asio::async_connect(
        socket_, std::move(results),
        [this](boost::system::error_code ec, auto connected_endpoint) {
            if (ec)
            {
                LOG(log, warning)
                    << "Connection error, could not connect to any address, "
                       "retrying in "
                    << conf_.connect_retry_interval.count() << "ms";
                this->hndl_.connect_error(ec);
                pool_->schedule(conf_.connect_retry_interval, [this] {
                    async_connect();
                    return false;
                });

                return;
            }

            // no error, we can proceed.
            LOG(log, info) << "Connected to " << connected_endpoint.address()
                           << ":" << connected_endpoint.port();
            this->hndl_.connected();
        });
}

} // namespace client
} // namespace blabla
