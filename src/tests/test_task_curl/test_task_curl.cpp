#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <task_curl/task_curl.h>

#include "mock_server.h"

#include <cassert>

using namespace nn;
using namespace nn::curl;

namespace
{

	class BackgroundServer
	{
	public:
		BackgroundServer(IRequestListener& listener)
			: sockets_()
			, server_(listener)
			, worker_(&MockServer::handle_one_connection, &server_)
		{
		}

		~BackgroundServer()
		{
			assert(server_.had_connection());
			assert(!server_.waiting_connection());
			worker_.join();
		}

		bool was_connection_handled()
		{
			return server_.had_connection();
		}

	private:
		SocketsInitializer sockets_;
		MockServer server_;
		std::thread worker_;
	};

	std::string ToString(const Buffer& data)
	{
		if (data.empty())
		{
			return std::string();
		}
		return std::string(data.begin(), data.end());
	}
} // namespace

TEST(TaskCurl, Simple_Get)
{
	struct Response : IRequestListener
	{
		virtual std::string on_get_request(std::string /*url*/) override
		{
			return "data";
		}
	};
	Response response;
	BackgroundServer server(response);

	Scheduler scheduler;
	Task<Buffer, CurlError> get = make_task(scheduler, CurlGet("localhost"));
	while (get.is_in_progress())
	{
		(void)scheduler.poll();
	}
	ASSERT_TRUE(get.get().has_value());
	ASSERT_EQ("data", ToString(get.get().value()));
}
