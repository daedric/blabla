#include <algorithm>
#include <chrono>
#include <execinfo.h>
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <thread>
#include <unistd.h>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <commonpp/core/LoggingInterface.hpp>
#include <commonpp/thread/Thread.hpp>

#include "blabla/Blabla.hpp"

CREATE_LOGGER(main_log, "main");

namespace po = boost::program_options;

struct Opts
{
    std::string addr;
    bool debug;

    blabla::ServiceConfiguration conf;
};

auto get_opts(int ac, char** av)
{
    Opts opts;
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help,h", "Print this help")
        ("addr", po::value<std::string>(&opts.addr)->default_value("0.0.0.0:10900"), "Address to bind")
        ("debug", po::value<bool>(&opts.debug)->default_value(false), "enable debug level")
        // clang-format on
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        exit(EXIT_FAILURE);
    }

    po::notify(vm);
    return opts;
}

int main(int ac, char** av)
{
    commonpp::core::init_logging();
    commonpp::core::enable_console_logging();

    auto opts = get_opts(ac, av);

    { // XXX
        opts.conf.service.addresses.emplace_back(
            blabla::ServiceConfiguration::Address{
                "0.0.0.0",
                20100,
            });
    }

    commonpp::core::set_logging_level(commonpp::info);
    if (opts.debug)
    {
        commonpp::core::set_logging_level(commonpp::trace);
        commonpp::core::auto_flush_console(true);
    }
    commonpp::thread::set_current_thread_name("MAIN");
    blabla::Service svc(opts.conf);
    svc.start();

    {
        boost::asio::io_service service;
        boost::asio::signal_set signals(service, SIGINT, SIGTERM);
        signals.async_wait(
            [](const boost::system::error_code& error, int signal_number) {
                if (!error)
                {
                    LOG(main_log, warning)
                        << "Signal " << strsignal(signal_number) << " received";
                }
                else
                {
                    LOG(main_log, warning)
                        << "Error occured while waiting for a signal: "
                        << error.message();
                }
            });

        service.run();
    }

    LOG(main_log, warning) << "Exiting";

    return 0;
}
