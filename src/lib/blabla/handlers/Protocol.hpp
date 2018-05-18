#pragma once

#include "proto/service.pb.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace blabla
{
namespace handlers
{

template <typename Dispatcher>
struct MessageCracker
{
    template <typename Context>
    void process(Context& ctx, std::vector<uint8_t>& buff)
    {
        google::protobuf::io::ArrayInputStream in(buff.data(), buff.size());
        services::blabla::DecodableMessage msg;
        if (!msg.ParseFromZeroCopyStream(&in))
        {
            GLOG(error) << "Received invalid payload";
            dispatcher().decoding_error();
            return;
        }

        switch (msg.type().type())
        {
        default:
            // fall-through
        case services::blabla::UNUSED:
            dispatcher().decoding_unknown_type();
            return;

        case services::blabla::PING:
            return handle<services::blabla::Ping>(ctx, buff);
        case services::blabla::PONG:
            return handle<services::blabla::Pong>(ctx, buff);
        case services::blabla::SUSCRIBE_REQUEST:
            return handle<services::blabla::SubscribeRequest>(ctx, buff);
        case services::blabla::MESSAGE:
            return handle<services::blabla::ProducerMessageHeader>(ctx, buff);

        case services::blabla::ERROR:
            // XXX: server should not receive errors.
            return;
        }
    }

private:
    template <typename T, typename Context>
    void handle(Context& ctx, std::vector<uint8_t>& buff)
    {
        static_assert(std::is_base_of<google::protobuf::Message, T>::value == true,
                      "T must be derived from google::protobuf::Message");
        static thread_local T msg;

        msg.Clear();
        google::protobuf::io::ArrayInputStream in(buff.data(), buff.size());
        if (!msg.ParseFromZeroCopyStream(&in))
        {
            GLOG(error) << "Received invalid payload";
            dispatcher().decoding_error();
            return;
        }

        DGLOG(trace) << "Got a message: " << msg.DebugString();
        dispatcher().handle(ctx, msg);
    }

private:
    Dispatcher& dispatcher()
    {
        return static_cast<Dispatcher&>(*this);
    }
};
} // namespace handlers
} // namespace blabla