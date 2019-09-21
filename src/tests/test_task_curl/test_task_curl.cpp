#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <task_curl/task_curl.h>

#include "mock_server.h"

#include <vector>

#include <cassert>
#include <cstdio>

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
		.WillRepeatedly(Return("data"));

	Scheduler scheduler;
	SocketsInitializer sockets;
	MockServer server(scheduler, response);

	auto server_task = server.start("127.0.0.1", 1255/*port*/, 1/*backlog*/);
	(void)scheduler.poll();

	auto get = make_task(scheduler
		, CurlGet()
			.set_url("localhost:1255/test")
			.set_verbose(true));

	while (get.is_in_progress())
	{
		(void)scheduler.poll();
	}
	server_task.try_cancel();
	while (scheduler.has_tasks())
	{
		(void)scheduler.poll();
	}
	ASSERT_TRUE(server_task.is_finished());
	ASSERT_TRUE(get.get().has_value());
	ASSERT_EQ("data", ToString(get.get().value()));
}

TEST(TaskCurl, Multiple_Get)
{
	struct Response : IRequestListener
	{
		MOCK_METHOD1(on_get_request, std::string (std::string));
	};
	Response response;
	EXPECT_CALL(response, on_get_request("/test"))
		.WillRepeatedly(Return("data"));

	const int N = 10;
	Scheduler scheduler;
	SocketsInitializer sockets;
	MockServer server(scheduler, response);

	auto server_task = server.start("127.0.0.1", 1256/*port*/, N/*backlog*/);
	(void)scheduler.poll();

	Scheduler curl_scheduler;

	std::vector<Task<Buffer, CurlError>> requests;
	requests.reserve(static_cast<std::size_t>(N));
	for (int i = 0; i < N; ++i)
	{
		requests.push_back(make_task(curl_scheduler
			, CurlGet()
				.set_url("localhost:1256/test")
				.set_verbose(false)));
	}
	while (curl_scheduler.has_tasks())
	{
		(void)scheduler.poll();
		(void)curl_scheduler.poll();
	}
	server_task.try_cancel();
	while (scheduler.has_tasks())
	{
		(void)scheduler.poll();
	}
	ASSERT_TRUE(server_task.is_finished());

	StartStats stats = server_task.get().value_or(server_task.get().error());
	const unsigned accepted_count = stats.accepted_sockets_count;
	unsigned valid_response = 0;
	for (auto& request : requests)
	{
		assert(request.is_finished());
		if (request.is_successful())
		{
			if (ToString(request.get().value()) == "data")
			{
				++valid_response;
			}
		}
	}
	
	std::printf(
		"Accepted clients: %u\n"
		"With response   : %u\n"
		"Error           : %i\n"
		, accepted_count, valid_response, stats.error);

	ASSERT_GT(valid_response, 0u);
	ASSERT_GT(accepted_count, 0u);
	ASSERT_EQ(valid_response, accepted_count);
}
