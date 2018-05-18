#pragma once

#include <mutex>

#include <commonpp/thread/ThreadPool.hpp>

#include <boost/asio/ip/tcp.hpp>

namespace blabla
{
namespace client
{

struct EventHandler
{
    virtual ~EventHandler() = default;

    virtual void connect_error(const boost::system::error_code&)
    {
    }

    virtual void connected()
    {
    }
};

struct BlablaClientConfiguration
{
    std::string host;
    int16_t port;

    bool sync_connect = true;

    std::chrono::milliseconds connect_retry_interval{500};
};

class Client
{
    enum class State
    {
        not_connected,
        connecting,
        connected,
    };

public:
    Client(EventHandler&);
    Client(EventHandler&, commonpp::thread::ThreadPool& pool);

    ~Client();

    // will connect if sync_connect == true
    void configure(BlablaClientConfiguration conf);

private:
    void sync_connect();
    void async_connect();
    void async_resolve(boost::system::error_code,
                       boost::asio::ip::tcp::resolver::results_type);
    void async_connect(boost::asio::ip::tcp::resolver::results_type);

private:
    EventHandler& hndl_;
    std::shared_ptr<commonpp::thread::ThreadPool> pool_;
    std::mutex sock_mutex_;
    boost::asio::ip::tcp::socket socket_;
    BlablaClientConfiguration conf_;
};

} // namespace client
} // namespace blabla
