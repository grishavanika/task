#include <gtest/gtest.h>
#include <rename_me/custom_task.h>

#include <type_traits>

using namespace nn;

namespace
{

	template<typename D = void>
	struct CustomTask
	{
		Status tick(const ExecutionContext&);
	};

	struct UnknownData {};

	static_assert(IsCustomTask<CustomTask<>, void>::value
		, "CustomTask<> is CustomTaskInterface<void>");
	static_assert(IsCustomTask<CustomTask<float>, float>::value
		, "CustomTask<float> is CustomTaskInterface<float>");

	struct WrongTickReturnTask
	{
		bool tick(const ExecutionContext&);
	};
	static_assert(!IsCustomTask<WrongTickReturnTask, void>::value
		, "WrongTickReturnTask is not CustomTaskInterface<void> "
		"because tick(const ExecutionContext&) should return Status");

	struct WrongTickParametersTask
	{
		Status tick();
	};
	static_assert(!IsCustomTask<WrongTickParametersTask, void>::value
		, "WrongTickParametersTask is not CustomTaskInterface<void> "
		"because tick() should accept bool");

	struct NotTask {};
	static_assert(!IsCustomTask<NotTask, void>::value
		, "NotTask is NOT CustomTaskInterface<void>");

	template<typename T>
	typename std::enable_if<IsCustomTask<T, void>::value, bool>::type
		CheckSFINAE(T)
	{
		return true;
	}

	template<typename T>
	typename std::enable_if<!IsCustomTask<T, void>::value, bool>::type
		CheckSFINAE(T)
	{
		return false;
	}

	template<typename T>
	auto ValidOnlyForTasks(T) -> CustomTaskInterface<T, void>
	{
	}

	template<typename T, typename = void>
	struct IsValid : std::false_type
	{
	};

	template<typename T>
	struct IsValid<T, decltype(ValidOnlyForTasks(T()))> : std::true_type
	{
	};

	static_assert(IsValid<CustomTask<>>::value
		, "ValidOnlyForTasks(CustomTask<>) should compile");
	static_assert(!IsValid<NotTask>::value
		, "ValidOnlyForTasks(NotTask() should NOT compile");
} // namespace

TEST(CustomTask, CustomTask_Checks_Work_On_Function_Overload)
{
	ASSERT_TRUE(CheckSFINAE(CustomTask<>()));
	ASSERT_FALSE(CheckSFINAE(NotTask()));
	(void)ValidOnlyForTasks(CustomTask<>());
}
