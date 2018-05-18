#include "Client.hpp"

#include <arpa/inet.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/utility/string_view.hpp>

#include <commonpp/core/LoggingInterface.hpp>

#include "proto/service.pb.h"

namespace blabla
{
namespace handlers
{

CREATE_LOGGER(client_logger, "handlers::client");

void Client::start(ClientManager* manager)
{
    DLOG(client_logger, debug) << peer() << " started";
    this->manager = manager;
    read_message(shared_from_this());
}

void Client::stop()
{
    return killme();
}

void Client::read_message(std::shared_ptr<Client> myself)
{
    std::lock_guard<std::mutex> l(mutex);
    boost::asio::async_read(
        socket_, boost::asio::buffer(size_buffer.buff),
        boost::bind(&Client::maybe_read_message_size, this, std::move(myself),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void Client::killme()
{
    DLOG(client_logger, debug) << "Killing connection: " << peer();
    {
        std::lock_guard<std::mutex> l(mutex);
        unsubscribe_all();

        boost::system::error_code ec;
        socket_.cancel(ec);
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    }

    manager->remove_connection(shared_from_this());
}

void Client::unsubscribe_all()
{
    DLOG(client_logger, debug)
        << peer() << " removing: " << active_subscriptions.size()
        << " active subscriptions";
    for (auto& sub : active_subscriptions)
    {
        sub->remove_client(*this);
    }
}

bool Client::handle_error(boost::system::error_code errc)
{
    if (errc)
    {
        if (errc == boost::asio::error::operation_aborted)
        {
            return false;
        }
        else if (errc == boost::asio::error::eof ||
                 errc == boost::asio::error::connection_reset)
        {
            DLOG(client_logger, info) << "Client: " << peer() << " disconnected";
            killme();
            return false;
        }
        else
        {
            LOG(client_logger, error)
                << "An error occured on client: " << peer() << ", killing it";
            killme();
            return false;
        }
    }

    return true;
}

static services::blabla::Error error(services::blabla::Error_ErrorType code,
                                     std::string msg)
{
    services::blabla::Error err;
    {
        err.mutable_header()->set_type(services::blabla::ERROR);
        err.set_type(code);
        err.set_description(std::move(msg));
    }
    return err;
}

static SingleOwnershipBuffer::SingleOwnershipBufferPtr
to_buffer(const google::protobuf::Message& msg)
{
    std::vector<uint8_t> buff;
    buff.resize(msg.ByteSize());
    msg.SerializeToArray(buff.data(), buff.size());
    return SingleOwnershipBuffer::allocate(std::move(buff));
}

static const auto HARD_MSG_SIZE_LIMIT = 15 * 1024 * 1024; // 15MB

void Client::maybe_read_message_size(std::shared_ptr<Client> myself,
                                     boost::system::error_code errc,
                                     std::size_t)
{
    if (!handle_error(std::move(errc)))
    {
        return;
    }

    size_buffer.size = ::ntohl(size_buffer.size);
    DLOG(client_logger, info) << "Message size to read: " << size_buffer.size;

    if (BOOST_UNLIKELY(size_buffer.size > HARD_MSG_SIZE_LIMIT))
    {
        std::string str = "Got a payload of: " + std::to_string(size_buffer.size) +
                          "B (>" + std::to_string(HARD_MSG_SIZE_LIMIT) + "B)";
        return this->send_error(to_buffer(error(
            services::blabla::Error_ErrorType_PAYLOAD_TOO_BIG, std::move(str))));
    }
    else if (BOOST_UNLIKELY(size_buffer.size == 0))
    {
        return this->send_error(
            to_buffer(error(services::blabla::Error_ErrorType_PAYLOAD_TOO_SHORT,
                            "Got a null payload")));
    }

    control_message_buffer.clear();
    control_message_buffer.resize(size_buffer.size);
    std::lock_guard<std::mutex> l(mutex);
    boost::asio::async_read(
        socket_, boost::asio::buffer(control_message_buffer),
        boost::bind(&Client::maybe_read_message, this, std::move(myself),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void Client::maybe_read_message(std::shared_ptr<Client> myself,
                                boost::system::error_code errc,
                                std::size_t)
{
    if (!handle_error(std::move(errc)))
    {
        return;
    }

    Client::DispatchContext ctx;
    ctx.myself = std::move(myself);
    MessageCracker::process(ctx, control_message_buffer);
}

void Client::decoding_error()
{
    LOG(client_logger, error)
        << "Invalid payload received from: " << peer() << ", disconnecting";
    return send_error(
        to_buffer(error(services::blabla::Error_ErrorType_INVALID_MESSAGE,
                        "Could not decode the message")));
}

void Client::decoding_unknown_type()
{
    LOG(client_logger, error) << "Invalid payload received from: " << peer()
                              << ", message type is unknown, disconnecting";
    return send_error(
        to_buffer(error(services::blabla::Error_ErrorType_UNKNOWN_TYPE,
                        "Unknown message type, is the server up-to-date?")));
}

template <typename Buffer>
void Client::send_error(Buffer buff)
{
    auto buffers = buff->to_buffers();
    std::lock_guard<std::mutex> l(mutex);
    boost::asio::async_write(
        socket_, buffers,
        [conn = shared_from_this(), buff = std::move(buff)](
            boost::system::error_code ec, std::size_t) { conn->killme(); });
}

template <typename Buffer>
void Client::send_impl(Buffer buff)
{
    DLOG(client_logger, trace) << "Send message to : " << peer();
    auto buffers = buff->to_buffers();
    std::lock_guard<std::mutex> l(mutex);
    boost::asio::async_write(
        socket_, buffers,
        [buff = std::move(buff)](boost::system::error_code ec, std::size_t) {
            // Do not handle error in write. a read call will
            // eventually detect that the socket is unusable.
            if (ec)
            {
                if (ec == boost::asio::error::operation_aborted ||
                    ec == boost::asio::error::eof ||
                    ec == boost::asio::error::connection_reset)
                {
                }
                else
                {
                    LOG(client_logger, error)
                        << "An error occured during write: " << ec.message();
                }
            }
        });
}

void Client::send(SharedBuffer::SharedBufferPtr buff)
{
    return send_impl(std::move(buff));
}

void Client::send(std::unique_ptr<SharedBufferWithSpecificMetadata> buff)
{
    return send_impl(std::move(buff));
}

template <>
void Client::handle(DispatchContext& ctx, services::blabla::Ping& ping)
{
    services::blabla::Pong pong;
    pong.set_correlation_id(ping.correlation_id());
    send_impl(to_buffer(pong));
    return read_message(std::move(ctx.myself));
}

template <>
void Client::handle(DispatchContext&, services::blabla::Pong&)
{
    // traces lag time.
}

template <>
void Client::handle(DispatchContext& ctx, services::blabla::SubscribeRequest& req)
{
    std::vector<handlers::Subscription> subscriptions_to_add;
    std::vector<boost::string_view> subscriptions_to_rm;

    for (auto& sub : *req.mutable_subscriptions())
    {
        switch (sub.type())
        {
        case services::blabla::SubscribeRequest_Subscription_Type_SUBSCRIBE:
        {
            subscriptions_to_add.emplace_back(sub.route_prefix(),
                                              sub.correlation_id());
            break;
        }
        case services::blabla::SubscribeRequest_Subscription_Type_UNSUBSCRIBE:
        {
            subscriptions_to_rm.emplace_back(sub.route_prefix());
            break;
        }
        case services::blabla::SubscribeRequest_Subscription_Type_UNSUBSCRIBE_ALL:
        {
            unsubscribe_all();
            break;
        }

        default:
        {
            return send_error(to_buffer(
                error(services::blabla::Error_ErrorType_UNKNOWN_OPERATION,
                      "Unknown subscription operation")));
        }
        }
    }

    auto new_subscriptions = manager->subscribe(subscriptions_to_add, this);
    auto obsolete_subscriptions = manager->unsubscribe(subscriptions_to_rm, this);

    {
        std::lock_guard<std::mutex> l(mutex);
        { // first delete outdated subscriptions.
            std::sort(active_subscriptions.begin(), active_subscriptions.end());

            auto begin_obsolete = obsolete_subscriptions.begin();
            auto end_obsolete = obsolete_subscriptions.end();
            std::sort(begin_obsolete, end_obsolete);

            auto it = std::remove_if(
                     active_subscriptions.begin(), active_subscriptions.end(),
                     [begin_obsolete, end_obsolete, this](SubscriptionNode*n) {
                         bool del =
                             std::binary_search(begin_obsolete, end_obsolete, n);
                         if (del)
                         {
                             n->remove_client(*this);
                         }

                         return del;
                     }),
                 end = active_subscriptions.end();

            if (it != end)
            {
                active_subscriptions.erase(it, end);
            }
        }

        // then inserts new one.
        active_subscriptions.insert(
            active_subscriptions.end(),
            std::make_move_iterator(new_subscriptions.begin()),
            std::make_move_iterator(new_subscriptions.end()));
    }

    return read_message(std::move(ctx.myself));
}

template <>
void Client::handle(DispatchContext& ctx,
                    services::blabla::ProducerMessageHeader& msg)
{
    raw_payload_buffer.clear();
    raw_payload_buffer.resize(msg.message_size());
    std::lock_guard<std::mutex> l(mutex);
    boost::asio::async_read(
        socket_, boost::asio::buffer(raw_payload_buffer),
        boost::bind(&Client::maybe_read_payload, this, std::move(ctx.myself),
                    msg.route(), boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
}

void Client::maybe_read_payload(std::shared_ptr<blabla::handlers::Client> myself,
                                std::string route,
                                boost::system::error_code errc,
                                std::size_t)
{
    if (!handle_error(std::move(errc)))
    {
        return;
    }

    manager->emit_to(std::move(route),
                     SharedBufferWithSpecificMetadata::create_from(
                         std::move(raw_payload_buffer)));
    return read_message(std::move(myself));
}

} // namespace handlers
} // namespace blabla