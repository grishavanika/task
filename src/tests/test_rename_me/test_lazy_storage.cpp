#include <gtest/gtest.h>
#include <rename_me/detail/lazy_storage.h>

#include <memory>

using namespace nn::detail;

namespace
{
	// Check EBO stuff
	struct Empty {};
	struct EmptyFinal final {};
	struct EmptyNoDefaultCtor
	{
		explicit EmptyNoDefaultCtor(int, int) {}
	};
	static_assert(std::is_empty<EmptyNoDefaultCtor>::value
		, "Must be empty for tests purpose");
	static_assert(!std::is_default_constructible<EmptyNoDefaultCtor>::value
		, "Must not be default constructible for tests purpose");

	struct EmptyNoMoveAssignment
	{
		EmptyNoMoveAssignment(EmptyNoMoveAssignment&&) = default;
		EmptyNoMoveAssignment(const EmptyNoMoveAssignment&) = default;
		EmptyNoMoveAssignment& operator=(const EmptyNoMoveAssignment&) = default;
		EmptyNoMoveAssignment& operator=(EmptyNoMoveAssignment&&) = delete;
	};
	static_assert(std::is_empty<EmptyNoMoveAssignment>::value
		, "Must be empty for tests purpose");
	static_assert(!std::is_move_assignable<EmptyNoMoveAssignment>::value
		, "Must not be move assignable for tests purpose");

	struct WithData
	{
		int _;
	};
	static_assert(sizeof(WithData) > 1 && !std::is_empty<WithData>::value
		, "Must have some data for tests purpose");

	template<typename T, typename Storage = LazyStorage<T>>
	constexpr bool HasEbo = std::conjunction_v<
		typename Storage::is_ebo, std::is_empty<Storage>>;

	static_assert(HasEbo<Empty>
		, "EBO should be enabled for Empty struct");
	static_assert(!HasEbo<EmptyNoDefaultCtor>
		, "Can't do lazy EBO for non-default-constructible Empty struct");
	static_assert(!HasEbo<EmptyNoMoveAssignment>
		, "Can't do lazy EBO for non-move-assignable Empty struct");
	static_assert(!HasEbo<EmptyFinal>
		, "Can't do EBO for final type");
	static_assert(!HasEbo<int>
		, "int has some data, no EBO");
	static_assert(!HasEbo<WithData>
		, "No EBO for struct with some data");

	// Check ability to save non-default constructible stuff
	static_assert(std::is_default_constructible<LazyStorage<EmptyNoDefaultCtor>>::value
		, "Even non-default-constructible type can be stored in LazyStorage<>");

	struct NoDefaultCtor
	{
		std::unique_ptr<int> value;
		bool& destructed;

		explicit NoDefaultCtor(std::unique_ptr<int> v, bool& d)
			: value(std::move(v))
			, destructed(d)
		{
		}

		NoDefaultCtor(NoDefaultCtor&&) = delete;
		NoDefaultCtor& operator=(NoDefaultCtor&&) = delete;
		NoDefaultCtor(const NoDefaultCtor&) = delete;
		NoDefaultCtor& operator=(const NoDefaultCtor&) = delete;

		~NoDefaultCtor()
		{
			destructed = true;
		}
	};

} // namespace

TEST(EboStorage, Can_Be_Created_For_Non_Default_Constructible_Type)
{
	bool destructed = false;
	{
		LazyStorage<NoDefaultCtor> data;
		ASSERT_FALSE(data.has_value());
		data.emplace_once(std::make_unique<int>(10), destructed);
		ASSERT_TRUE(data.has_value());
		ASSERT_EQ(10, *data.get().value);
	}
	ASSERT_TRUE(destructed);
}
