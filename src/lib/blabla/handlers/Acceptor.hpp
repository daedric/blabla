#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <commonpp/core/LoggingInterface.hpp>
#include <commonpp/thread/ThreadPool.hpp>

namespace blabla
{
namespace handlers
{

struct Acceptor
{
    using tcp = boost::asio::ip::tcp;

public:
    Acceptor(commonpp::thread::ThreadPool& pool,
             boost::asio::ip::address address,
             int port)
    : pool(pool)
    , acceptor(pool.getService(), tcp::endpoint(std::move(address), port))
    {
    }

    template <typename Client, typename CB>
    void start(CB callback)
    {
        running = true;
        auto client = Client::create(pool);
        auto& sock = socket(client);
        acceptor.async_accept(
            sock, [this, client = std::move(client), cb = std::move(callback)](
                      const boost::system::error_code& error) mutable {
                if (!error)
                {
                    socket(client).non_blocking(true);
                    cb(client);
                    start<Client>(std::move(cb));
                    return;
                }

                if (error == boost::asio::error::operation_aborted)
                {
                    GLOG(warning) << "Stopping acceptor";
                }
                else
                {
                    GLOG(warning) << "Error during accept: " << error.message();
                }

                running = false;
            });
    }

    void stop()
    {
        if (running)
        {
            acceptor.cancel();
            acceptor.close();
            while (running != false)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                std::this_thread::yield();
            }
        }
    }

    ~Acceptor()
    {
        stop();
    }

    commonpp::thread::ThreadPool& pool;
    boost::asio::ip::tcp::acceptor acceptor;
    std::atomic_bool running{false};
};

} // namespace handlers
} // namespace blabla
