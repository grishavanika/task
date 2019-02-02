#pragma once
#include <rename_me/task.h>

#include <string>
#include <memory>
#include <atomic>

#include <cstdint>

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

struct StartStats
{
	unsigned accepted_sockets_count = 0;
	int error = 0;
};

class MockServer
{
public:
	explicit MockServer(nn::Scheduler& scheduler
		, IRequestListener& listener);
	~MockServer();

	nn::Task<StartStats, StartStats> start(const std::string& address, std::uint16_t port
		, int backlog = 1);

private:
	void on_new_connection(::detail::TcpSocket&& client);
	nn::Task<StartStats, StartStats> accept_forever();

private:
	nn::Scheduler& scheduler_;
	IRequestListener& listener_;
	std::unique_ptr<::detail::TcpSocket> socket_;
};
