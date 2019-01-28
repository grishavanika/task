#pragma once
#include <string>
#include <memory>
#include <atomic>

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
	explicit MockServer(nn::Scheduler& scheduler, IRequestListener& listener);
	~MockServer();

	// Blocking. Can be called only once
	void handle_one_connection();

	bool had_connection() const;
	bool waiting_connection() const;

private:
	nn::Task<void, int> start(const std::string& address, std::uint16_t port);

private:
	nn::Scheduler& scheduler_;
	IRequestListener& listener_;
	std::unique_ptr<detail::TcpSocket> socket_;
	std::atomic_bool did_accept_;
	std::atomic_bool waiting_accept_;
};
