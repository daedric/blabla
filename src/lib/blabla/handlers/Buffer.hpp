#pragma once

#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <memory>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/utility.hpp>

namespace blabla
{
namespace handlers
{

union IntBuffer {
    uint32_t size;
    char buff[sizeof(uint32_t)];
};

namespace detail
{
template <typename... Args>
static auto asio_buffers(Args&&... args)
{
    return std::array<boost::asio::const_buffer, sizeof...(Args)>({
        boost::asio::buffer(std::forward<Args>(args))...,
    });
}
} // namespace detail

struct SingleOwnershipBuffer
{
private:
private:
    SingleOwnershipBuffer(std::vector<uint8_t> buffer)
    : size()
    , buffer(std::move(buffer))
    {
        size.size = ::htonl(static_cast<uint32_t>(this->buffer.size()));
        buffers = detail::asio_buffers(size.buff, this->buffer);
    }

public:
    using SingleOwnershipBufferPtr = std::unique_ptr<SingleOwnershipBuffer>;
    static SingleOwnershipBufferPtr allocate(std::vector<uint8_t> buffer)
    {
        return SingleOwnershipBufferPtr(
            new SingleOwnershipBuffer(std::move(buffer)));
    }

public:
    auto to_buffers() const
    {
        return buffers;
    }

private:
    IntBuffer size;
    std::vector<uint8_t> buffer;
    std::array<boost::asio::const_buffer, 2> buffers;
};

struct SharedBuffer : private boost::noncopyable
{
private:
    SharedBuffer(std::vector<uint8_t> buffer)
    : size()
    , buffer(std::move(buffer))
    {
        size.size = ::htonl(static_cast<uint32_t>(this->buffer.size()));
        buffers = detail::asio_buffers(size.buff, this->buffer);
    }

public:
    using SharedBufferPtr = std::shared_ptr<SharedBuffer>;
    static SharedBufferPtr allocate(std::vector<uint8_t> buffer)
    {
        return SharedBufferPtr(new SharedBuffer(std::move(buffer)));
    }

public:
    auto to_buffers() const
    {
        return buffers;
    }

private:
    IntBuffer size;
    std::vector<uint8_t> buffer;
    std::array<boost::asio::const_buffer, 2> buffers;
};

struct SharedBufferWithSpecificMetadata
{
    static std::unique_ptr<SharedBufferWithSpecificMetadata>
    create_from(std::vector<uint8_t> immutable_data)
    {
        auto result = std::make_unique<SharedBufferWithSpecificMetadata>();
        result->immutable_buffer = std::make_shared<Buffer>();
        result->immutable_buffer->buffer = std::move(immutable_data);
        return result;
    }

    std::unique_ptr<SharedBufferWithSpecificMetadata>
    new_with_metadata(std::vector<uint8_t> metadata)
    {
        auto result = std::make_unique<SharedBufferWithSpecificMetadata>(*this);
        result->metadata = std::move(metadata);
        result->size.size = ::htonl(result->metadata.size());

        result->_buffers =
            detail::asio_buffers(result->size.buff, result->metadata,
                                 result->immutable_buffer->buffer);

        return result;
    }

    size_t payload_size() const
    {
        assert(immutable_buffer != nullptr);
        return immutable_buffer->buffer.size();
    }

    auto to_buffers() const
    {
        return _buffers;
    }

private:
    struct Buffer
    {
        std::vector<uint8_t> buffer;
    };

private:
    IntBuffer size{};
    std::vector<uint8_t> metadata;
    std::shared_ptr<Buffer> immutable_buffer;
    std::array<boost::asio::const_buffer, 3> _buffers;
};

} // namespace handlers
} // namespace blabla