#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <task_curl/task_curl.h>

#include "mock_server.h"

#include <cassert>

using namespace nn;
using namespace nn::curl;

using ::testing::Return;

namespace
{
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
		MOCK_METHOD1(on_get_request, std::string (std::string));
	};
	Response response;
	EXPECT_CALL(response, on_get_request("/test"))
		.WillOnce(Return("data"));

	Scheduler scheduler;
	SocketsInitializer sockets;
	MockServer server(scheduler, response, "127.0.0.1", 1255);

	auto receive = server.handle_one_connection();
	(void)scheduler.poll();

	auto get = make_task(scheduler, CurlGet("localhost:1255/test"));
	while (scheduler.has_tasks())
	{
		(void)scheduler.poll();
	}
	ASSERT_TRUE(receive.is_successful())
		<< "Server error: " << receive.get().error();
	ASSERT_TRUE(get.get().has_value());
	ASSERT_EQ("data", ToString(get.get().value()));
}
