#include <gtest/gtest.h>
#include <task_curl/task_curl.h>

TEST(TaskCurl, Dummy)
{
	ASSERT_EQ(1, task_curl());
}
