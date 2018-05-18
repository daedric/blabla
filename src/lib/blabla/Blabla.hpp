#pragma once

#include <thread>

#include <commonpp/thread/ThreadPool.hpp>

namespace blabla
{

struct ServiceConfiguration
{

    struct Address
    {
        std::string address;
        int port;
    };

    struct
    {
        std::vector<Address> addresses;
    } service;

    struct
    {
        int io_threads = std::thread::hardware_concurrency();
        int io_context = commonpp::thread::get_nb_physical_core();
    } threads;
};

namespace detail
{
struct Service;
}

class Service final
{

public:
    Service(ServiceConfiguration conf = {});
    ~Service();

    void start();

private:
    std::unique_ptr<detail::Service> service;
    const ServiceConfiguration conf;
    commonpp::thread::ThreadPool pool;
};

} // namespace blabla
