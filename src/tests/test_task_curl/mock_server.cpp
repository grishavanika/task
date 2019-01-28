#include "mock_server.h"

#include <rename_me/scheduler.h>
#include <rename_me/function_task.h>
#include <rename_me/task_forward.h>

#include <sstream>

#include <cstdint>
#include <cassert>
#include <cstdlib>

// Evaluate condition 2 time in Debug.
// Needed to see expression text in the assert() on fail.
#define NN_ENSURE(COND) \
	{ \
		assert(COND); \
		if (!(COND)) abort(); \
	}

#if defined(_WIN32)
#  define _WINSOCK_DEPRECATED_NO_WARNINGS
#  include <WinSock2.h>
namespace
{
	using Socket = SOCKET;
	const Socket kInvalidSocket = INVALID_SOCKET;

	void CloseSocketImpl(Socket s)
	{
		const int status = ::closesocket(s);
		NN_ENSURE(status == 0);
	}

	void ShutdownImpl(Socket s)
	{
		const int status = ::shutdown(s, SD_BOTH);
		NN_ENSURE(status == 0);
	}
} // namespace
#else
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <sys/types.h>
#  include <sys/socket.h>

namespace
{
	using Socket = int;
	const Socket kInvalidSocket = -1;
	
	void CloseSocketImpl(SOCKET s)
	{
		const int status = ::close(s);
		NN_ENSURE(status == 0);
	}

	void ShutdownImpl(Socket s)
	{
		const int status = ::shutdown(s, SHUT_RDWR);
		NN_ENSURE(status == 0);
	}
} // namespace
#endif

namespace
{

	nn::expected<void, int> MakeExpectedFromStatus(int status, int success = 0)
	{
		if (status == success)
		{
			return nn::expected<void, int>();
		}
		return nn::expected<void, int>(nn::unexpected<int>(status));
	}

	void SetAsyncSocket(Socket s)
	{
		u_long async = 1;
		const int status = ::ioctlsocket(s, FIONBIO, &async);
		NN_ENSURE(status == NO_ERROR);
	}

} // namespace

namespace detail
{

	class TcpSocket
	{
	public:
		explicit TcpSocket(nn::Scheduler& scheduler)
			: TcpSocket(scheduler, ::socket(AF_INET, SOCK_STREAM, 0))
		{
			NN_ENSURE(is_valid());
			SetAsyncSocket(socket_);
		}


		TcpSocket(const TcpSocket&) = delete;
		TcpSocket& operator=(const TcpSocket&) = delete;

		TcpSocket(TcpSocket&& rhs)
			: scheduler_(rhs.scheduler_)
			, socket_(rhs.socket_)
		{
			rhs.socket_ = kInvalidSocket;
		}

		TcpSocket& operator=(TcpSocket&& rhs)
		{
			if (this != &rhs)
			{
				close();
				scheduler_ = rhs.scheduler_;
				socket_ = rhs.socket_;
				rhs.socket_ = kInvalidSocket;
			}
			return *this;
		}

		nn::Task<void, int> bind(const std::string& address, std::uint16_t port)
		{
			return nn::make_task(*scheduler_, [=]()
			{
				sockaddr_in addr{};
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = ::inet_addr(address.c_str());
				addr.sin_port = htons(port);
				const int status = ::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
				return MakeExpectedFromStatus(status);
			});
		}

		nn::Task<void, int> listen(int queue_len = 1)
		{
			return nn::make_task(*scheduler_, [=]()
			{
				const int status = ::listen(socket_, queue_len);
				return MakeExpectedFromStatus(status);
			});
		}

		void close()
		{
			if (is_valid())
			{
				ShutdownImpl(socket_);
				CloseSocketImpl(socket_);
				socket_ = kInvalidSocket;
			}
		}

		bool is_valid() const
		{
			return (socket_ != kInvalidSocket);
		}

		nn::Task<TcpSocket, int> accept()
		{
			struct AcceptTask
			{
				TcpSocket* server_;
				nn::expected<TcpSocket, int> result_;

				AcceptTask(TcpSocket* server)
					: server_(server)
					, result_(TcpSocket())
				{
				}

				nn::Status tick(bool cancel)
				{
					if (cancel)
					{
						return nn::Status::Canceled;
					}
					sockaddr_in addr{};
					int addr_len = sizeof(addr);
					Socket client = ::accept(server_->socket_
						, reinterpret_cast<sockaddr*>(&addr), &addr_len);
					const int error = ::WSAGetLastError();
					if (client != kInvalidSocket)
					{
						result_ = TcpSocket(*server_->scheduler_, client);
						return nn::Status::Successful;
					}

					// http://man7.org/linux/man-pages/man2/accept.2.html
					// [...] accept() fails with the error EAGAIN or EWOULDBLOCK.
					if (error == WSAEWOULDBLOCK)
					{
						return nn::Status::InProgress;
					}
					result_ = nn::unexpected<int>(error);
					return nn::Status::Failed;
				}

				nn::expected<TcpSocket, int>& get()
				{
					return result_;
				}
			};

			return nn::Task<TcpSocket, int>::make<AcceptTask>(
				*scheduler_, this);
		}

		std::string receive_once()
		{
			std::string chunk;
			chunk.resize(1024);
			// Read available (up to 1024 bytes) data
			const int available = ::recv(socket_, chunk.data()
				, static_cast<int>(chunk.size()), MSG_PEEK);
			if (available == 0)
			{
				// Connection has been gracefully closed
				return std::string();
			}
			NN_ENSURE(available != SOCKET_ERROR);
			NN_ENSURE(available <= static_cast<int>(chunk.size()));
			chunk.resize(available);
			return chunk;
		}

		void send(std::string data)
		{
			const int length = static_cast<int>(data.size());
			if (length == 0)
			{
				return;
			}
			const int sent = ::send(socket_, data.data(), length, 0);
			NN_ENSURE(sent == length);
		}

		~TcpSocket()
		{
			close();
		}

	private:
		explicit TcpSocket()
			: scheduler_(nullptr)
			, socket_(kInvalidSocket)
		{
		}

		explicit TcpSocket(nn::Scheduler& scheduler, Socket socket)
			: scheduler_(&scheduler)
			, socket_(socket)
		{
		}

	private:
		nn::Scheduler* scheduler_;
		Socket socket_;
	};

} // namespace detail

namespace
{

	struct GetRequest
	{
		std::string url;
	};

	GetRequest ParseGetRequest(std::string data)
	{
		std::string line;
		{
			std::istringstream s(std::move(data));
			std::getline(s, line);
		}
		std::istringstream s(std::move(line));
		{
			std::string get;
			s >> get;
			if (get != "GET")
			{
				return GetRequest();
			}
		}
		GetRequest request;
		s >> request.url;
		{
			std::string protocol;
			s >> protocol;
			if (protocol != "HTTP/1.1")
			{
				return GetRequest();
			}
		}
		return request;
	}

} // namespace

SocketsInitializer::SocketsInitializer()
{
#if defined(_WIN32)
	WSADATA wsa{};
	const auto status = ::WSAStartup(MAKEWORD(2, 2), &wsa);
	NN_ENSURE(status == NO_ERROR);
#endif
}

SocketsInitializer::~SocketsInitializer()
{
#if defined(_WIN32)
	(void)::WSACleanup();
#endif
}

/*explicit*/ MockServer::MockServer(
	nn::Scheduler& scheduler, IRequestListener& listener)
	: scheduler_(scheduler)
	, listener_(listener)
	, socket_(std::make_unique<detail::TcpSocket>(scheduler_))
	, did_accept_(false)
	, waiting_accept_(false)
{
}

nn::Task<void, int> MockServer::start(const std::string& address, std::uint16_t port)
{
	nn::forward_error(
		socket_->bind(address, port)
		, [=]()
	{
		//return nn::make_task(nn::success, scheduler_);
		return socket_->listen();
	});

	(void)address;
	(void)port;
	return nn::make_task<int>(nn::success, scheduler_);
}

void MockServer::handle_one_connection()
{
	NN_ENSURE(!waiting_connection());
	NN_ENSURE(!had_connection());

	//(void)start("127.0.0.1", 80)
	//	.then([=](const nn::Task<void, int>& start_task) mutable
	//{
	//	return nn::forward_success(start_task, [=]()
	//	{
	//		return socket_->accept();
	//	});
	//})
	//	.then([=](const nn::Task<detail::TcpSocket, int>& accept_task)
	//{
	//	return nn::make_task(scheduler_, accept_task.get_once());
	//});

	//auto client = socket_->accept();
	//waiting_accept_ = false;
	//did_accept_ = true;

	//if (!client.is_valid())
	//{
	//	return;
	//}

	//auto request = ParseGetRequest(client.receive_once());
	//if (request.url.empty())
	//{
	//	return;
	//}
	//auto response = listener_.on_get_request(std::move(request.url));
	//client.send(std::move(response));
}

bool MockServer::had_connection() const
{
	return did_accept_;
}

bool MockServer::waiting_connection() const
{
	return waiting_accept_;
}

MockServer::~MockServer()
{
	NN_ENSURE(!waiting_connection());
}
