#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <rename_me/in_place_task.h>

#include "test_tools.h"

using namespace nn;

TEST(Inplace, Task_With_No_Args_Is_Like_Noop)
{
	Scheduler sch;
	Task<> noop = make_in_place_task(sch);
	ASSERT_EQ(1u, sch.poll());
	ASSERT_TRUE(noop.is_successful());
}
