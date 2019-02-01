#pragma once
#include <string>
#include <memory>
#include <atomic>

#include <cstdint>

namespace nn
{
	class Scheduler;

	template<typename T, typename E>
	class Task;
} // namespace nn

class IRequestListener
{
public:
	virtual std::string on_get_request(std::string url) = 0;

protected:
	~IRequestListener() = default;
};

namespace detail
{
	class TcpSocket;
} // namespace detail

struct SocketsInitializer
{
	SocketsInitializer();
	~SocketsInitializer();
	
	SocketsInitializer(const SocketsInitializer&) = delete;
	SocketsInitializer(SocketsInitializer&&) = delete;
	SocketsInitializer& operator=(const SocketsInitializer&) = delete;
	SocketsInitializer& operator=(SocketsInitializer&&) = delete;
};

class MockServer
{
public:
	explicit MockServer(nn::Scheduler& scheduler
		, IRequestListener& listener
		, std::string address
		, std::uint16_t port);
	~MockServer();

	nn::Task<void, int> handle_one_connection();

private:
	nn::Scheduler& scheduler_;
	IRequestListener& listener_;
	std::unique_ptr<::detail::TcpSocket> socket_;
	std::string address_;
	std::uint16_t port_;
};
