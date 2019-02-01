#include "mock_server.h"

#include <rename_me/scheduler.h>
#include <rename_me/function_task.h>
#include <rename_me/task_forward.h>

#include <sstream>

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
		(void)status;
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

		nn::Task<std::string, int> receive_once() &
		{
			struct ReceiveTask
			{
				Socket client_;
				nn::expected<std::string, int> result_;

				ReceiveTask(TcpSocket& client)
					: client_(client.socket_)
					, result_(std::string())
				{
					result_.value().resize(1024);
				}

				nn::Status tick(bool cancel)
				{
					if (cancel)
					{
						return nn::Status::Canceled;
					}
					std::string& chunk = result_.value();
					const int available = ::recv(client_
						, chunk.data()
						, static_cast<int>(chunk.size()), 0);
					const int error = ::WSAGetLastError();
					if ((available == SOCKET_ERROR) && error == WSAEWOULDBLOCK)
					{
						return nn::Status::InProgress;
					}
					else if (available == SOCKET_ERROR)
					{
						result_ = nn::unexpected<int>(error);
						return nn::Status::Failed;
					}
					chunk.resize(available);
					return nn::Status::Successful;
				}

				nn::expected<std::string, int>& get()
				{
					return result_;
				}
			};

			return nn::Task<std::string, int>::make<ReceiveTask>(
				*scheduler_, *this);
		}

		nn::Task<void, int> send_once(std::string data) &&
		{
			const int length = static_cast<int>(data.size());
			if (length == 0)
			{
				return nn::make_task<int>(nn::success, *scheduler_);
			}
			const int sent = ::send(socket_, data.data(), length, 0);
			assert(sent == length);
			// #TODO: async
			return nn::make_task<int>(nn::success, *scheduler_);
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
		// Cool "GET / HTTP/1.1" request parsing
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

	std::string MakeResponse(std::string data)
	{
		// Dump response creating
		std::string response;
		response += "HTTP/1.0 200 OK\n";
		response += "Server: MockServer\n";
		response += "Content-type: text/plain; charset=UTF-8\n";
		response += "Content-Length: " + std::to_string(data.size()) + "\n";
		response += "\n";
		response += data;
		return response;
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
	nn::Scheduler& scheduler
	, IRequestListener& listener
	, std::string address /*= "127.0.0.1"*/
	, std::uint16_t port /*= 80*/)
	: scheduler_(scheduler)
	, listener_(listener)
	, socket_(std::make_unique<detail::TcpSocket>(scheduler_))
	, address_(std::move(address))
	, port_(port)
{
}

nn::Task<void, int> MockServer::handle_one_connection()
{
	auto start = [=]() -> nn::Task<void, int>
	{
		return nn::forward_error(
			socket_->bind(address_, port_)
			, [=] { return socket_->listen(); });
	};
	auto accept = [=]() -> nn::Task<::detail::TcpSocket, int>
	{
		return nn::forward_error(start()
			, [=] { return socket_->accept(); });
	};
	
	return nn::forward_error(accept()
		, [&](::detail::TcpSocket&& client)
	{
		auto receive = client.receive_once();
		return nn::forward_error(std::move(receive)
			, [this, client = std::move(client)]
				(std::string&& payload) mutable
		{
			auto request = ParseGetRequest(std::move(payload));
			if (request.url.empty())
			{
				// #TODO: nice error
				return nn::make_task(nn::error, scheduler_, 1);
			}
			auto response = MakeResponse(listener_.on_get_request(std::move(request.url)));
			return std::move(client).send_once(std::move(response));
		});
	});
}

MockServer::~MockServer()
{
}
