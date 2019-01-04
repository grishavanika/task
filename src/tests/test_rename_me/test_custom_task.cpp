#include <gtest/gtest.h>
#include <rename_me/custom_task.h>

#include <type_traits>

using namespace nn;

namespace
{

	template<typename T = void, typename E = void>
	struct CustomTask
	{
		Status tick(bool cancel_requested);
		expected<T, E>& get();
	};

	static_assert(IsCustomTask<CustomTask<>, void, void>::value
		, "CustomTask<> is CustomTaskInterface<void, void>");
	static_assert(IsCustomTask<CustomTask<float, char>, float, char>::value
		, "CustomTask<float, char> is CustomTaskInterface<float, char>");

	static_assert(!IsCustomTask<CustomTask<int, int>, void, void>::value
		, "CustomTask<int, int> is NOT CustomTaskInterface<void, void>");
	static_assert(!IsCustomTask<char, void, void>::value
		, "char is NOT CustomTaskInterface<void, void>");

	struct WrongTickReturnTask
	{
		bool tick(bool cancel_requested);
		expected<void, void>& get();
	};
	static_assert(!IsCustomTask<WrongTickReturnTask, void, void>::value
		, "WrongTickReturnTask is not CustomTaskInterface<void, void> "
		"because tick(bool) should return Status");

	struct WrongTickParametersTask
	{
		Status tick();
		expected<void, void>& get();
	};
	static_assert(!IsCustomTask<WrongTickParametersTask, void, void>::value
		, "WrongTickParametersTask is not CustomTaskInterface<void, void> "
		"because tick() should accept bool");

	struct WrongGetReturnTask
	{
		Status tick(bool cancel_requested);
		expected<void, void> get();
	};
	static_assert(!IsCustomTask<WrongGetReturnTask, void, void>::value
		, "WrongGetReturnTask is not CustomTaskInterface<void, void> "
		"because get() should return expected<void, void>&");

	struct WrongGetParametersTask
	{
		Status tick();
		expected<void, void>& get(int);
	};
	static_assert(!IsCustomTask<WrongGetParametersTask, void, void>::value
		, "WrongGetParametersTask is not CustomTaskInterface<void, void> "
		"because get() has not any parameter");

	struct NotTask {};
	static_assert(!IsCustomTask<NotTask, void, void>::value
		, "NotTask is NOT CustomTaskInterface<void, void>");

	template<typename T>
	typename std::enable_if<IsCustomTask<T, void, void>::value, bool>::type
		CheckSFINAE(T)
	{
		return true;
	}

	template<typename T>
	typename std::enable_if<!IsCustomTask<T, void, void>::value, bool>::type
		CheckSFINAE(T)
	{
		return false;
	}

	template<typename T>
	auto ValidOnlyForTasks(T) -> CustomTaskInterface<T, void, void>
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
