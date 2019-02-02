#include "mock_server.h"

#include <rename_me/scheduler.h>
#include <rename_me/function_task.h>
#include <rename_me/forward_task.h>
#include <rename_me/for_loop_task.h>
#include <rename_me/in_place_task.h>

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
#  include <WS2tcpip.h>

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

	void SetAsyncSocket(Socket s, bool enable = true)
	{
		u_long async = enable ? 1 : 0;
		const int status = ::ioctlsocket(s, FIONBIO, &async);
		NN_ENSURE(status == NO_ERROR);
	}

	int LastSocketError()
	{
		return ::WSAGetLastError();
	}

	bool IsSocketNonblockingError(int error)
	{
		return ((error == WSAEWOULDBLOCK) || (error == EAGAIN));
	}

} // namespace
#else
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>

namespace
{
	using Socket = int;
	const Socket kInvalidSocket = -1;
	const int SOCKET_ERROR = -1;

	void CloseSocketImpl(Socket s)
	{
		const int status = ::close(s);
		NN_ENSURE(status == 0);
	}

	void ShutdownImpl(Socket s)
	{
		const int status = ::shutdown(s, SHUT_RDWR);
		(void)status;
	}

	void SetAsyncSocket(Socket s, bool enable = true)
	{
		int flags = ::fcntl(s, F_GETFL, 0);
		NN_ENSURE(flags != 0);
		flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
		const int status = ::fcntl(s, F_SETFL, flags);
		NN_ENSURE(status == 0);
	}

	int LastSocketError()
	{
		return errno;
	}

	bool IsSocketNonblockingError(int error)
	{
		return ((error == EWOULDBLOCK) || (error == EAGAIN));
	}

} // namespace
#endif

namespace
{

	nn::expected<void, int> MakeExpectedFromStatus(int status, int error)
	{
		if (status != SOCKET_ERROR)
		{
			return nn::expected<void, int>();
		}
		return nn::MakeExpectedWithError<nn::expected<void, int>>(error);
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
				return MakeExpectedFromStatus(status, LastSocketError());
			});
		}

		nn::Task<void, int> listen(int queue_len)
		{
			return nn::make_task(*scheduler_, [=]()
			{
				const int status = ::listen(socket_, queue_len);
				return MakeExpectedFromStatus(status, LastSocketError());
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

		nn::Task<TcpSocket, int> accept() &
		{
			struct AcceptContext
			{
				Socket socket;
				nn::expected<TcpSocket, int> result;
				AcceptContext(Socket s) : socket(s), result(TcpSocket()) {}
			};
			return nn::make_in_place_task(
				nn::make_context(*scheduler_, AcceptContext(socket_))
				, [](nn::Context<AcceptContext>& context, bool cancel)
			{
				if (cancel)
				{
					SetExpectedWithError(context.data().result, 0);
					return nn::Status::Canceled;
				}
				sockaddr_in addr{};
				socklen_t addr_len = sizeof(addr);
				Socket client = ::accept(context.data().socket
					, reinterpret_cast<sockaddr*>(&addr), &addr_len);
				const int error = LastSocketError();
				if (client != kInvalidSocket)
				{
					context.data().result = TcpSocket(context.scheduler(), client);
					return nn::Status::Successful;
				}
				else if (IsSocketNonblockingError(error))
				{
					return nn::Status::InProgress;
				}
				SetExpectedWithError(context.data().result, error);
				return nn::Status::Failed;
			}
				, [](nn::Context<AcceptContext>& context)
			{
				return std::move(context.data().result);
			});
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
						SetExpectedWithError(result_, 0);
						return nn::Status::Canceled;
					}
					std::string& chunk = result_.value();
					const int available = ::recv(client_
						, chunk.data()
						, static_cast<int>(chunk.size()), 0);
					const int error = LastSocketError();
					if ((available == SOCKET_ERROR) && IsSocketNonblockingError(error))
					{
						return nn::Status::InProgress;
					}
					else if (available == SOCKET_ERROR)
					{
						SetExpectedWithError(result_, error);
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
			(void)sent;
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
	, IRequestListener& listener)
	: scheduler_(scheduler)
	, listener_(listener)
	, socket_(std::make_unique<detail::TcpSocket>(scheduler_))
{
}

void MockServer::on_new_connection(::detail::TcpSocket&& client)
{
	auto receive = client.receive_once();
	(void)nn::forward_error(std::move(receive)
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
}

nn::Task<StartStats, StartStats> MockServer::accept_forever()
{
	return nn::make_forever_loop_task(
		nn::make_loop_context(scheduler_, StartStats())
		, [this](nn::LoopContext<StartStats>&)
	{
		return socket_->accept();
	}
		, [this](nn::LoopContext<StartStats>& context
			, const nn::Task<::detail::TcpSocket, int>& accept)
	{
		if (accept.is_successful())
		{
			++context.data().accepted_sockets_count;
			on_new_connection(std::move(accept.get().value()));
			return true;
		}
		else if (accept.is_canceled())
		{
			return false;
		}
		else if (accept.is_failed())
		{
			context.data().error = accept.get().error();
			return false;
		}
		return true;
	}
		, [](nn::LoopContext<StartStats>& context
			, const nn::Task<::detail::TcpSocket, int>&, nn::Status status)
	{
		using Result = nn::expected<StartStats, StartStats>;
		if (status == nn::Status::Successful)
		{
			return Result(std::move(context.data()));
		}
		return nn::MakeExpectedWithError<Result>(std::move(context.data()));
	});
}

nn::Task<StartStats, StartStats> MockServer::start(const std::string& address
	, std::uint16_t port, int backlog /*= 1*/)
{
	auto start = [this, backlog, start_task = socket_->bind(address, port)]() mutable
	{
		return nn::forward_error(std::move(start_task)
			, [this, backlog] { return socket_->listen(backlog); });
	};
	return start().then([this](const nn::Task<void, int>& listen)
	{
		if (listen.is_successful())
		{
			return accept_forever();
		}
		assert(!listen.is_canceled()
			&& "Unhandled case: listen.get().error() may not be valid");
		StartStats stats;
		stats.error = listen.get().error();
		using Result = nn::expected<StartStats, StartStats>;
		return nn::make_task(scheduler_, Result(stats));
	});
}

MockServer::~MockServer()
{
}
