#include <iostream>

#include <boost/program_options.hpp>
#include <commonpp/core/LoggingInterface.hpp>
#include <commonpp/thread/ThreadPool.hpp>

#include <blabla/client/Client.hpp>

namespace po = boost::program_options;

struct Handler : blabla::client::EventHandler
{
};

int main(int ac, char** av)
{
    commonpp::core::init_logging();
    commonpp::core::enable_console_logging();

    commonpp::thread::ThreadPool pool(2);
    blabla::client::BlablaClientConfiguration conf;

    Handler handler;
    blabla::client::Client cl(handler, pool);

    {
        conf.host = "localhost";
        conf.port = 20100;
        conf.sync_connect = false;

        cl.configure(std::move(conf));
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
}
