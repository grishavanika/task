#include <gtest/gtest.h>
#include <rename_me/detail/ref_count_ptr.h>

#include <cstdint>

using namespace nn::detail;

namespace
{
	struct RefCountedBase
	{
		std::int8_t ref_;
		int& dead_count;

		// Pointer interface
		bool remove_ref_count() noexcept { return (--ref_ == 0); }
		void add_ref_count() noexcept { ++ref_; }

		RefCountedBase(int& dead)
			: ref_(1)
			, dead_count(dead)
		{
			dead_count = 0;
		}

		RefCountedBase(RefCountedBase&&) = delete;
		RefCountedBase& operator=(RefCountedBase&&) = delete;
		RefCountedBase(const RefCountedBase&) = delete;
		RefCountedBase& operator=(const RefCountedBase&) = delete;

		~RefCountedBase()
		{
			++dead_count;
		}
	};

	struct RefCounted : RefCountedBase
	{
		using RefCountedBase::RefCountedBase;
	};

	using BasePtr = RefCountPtr<RefCountedBase>;
	using Ptr = RefCountPtr<RefCounted>;
} // namespace

TEST(RefCountPtr, Destroys_Object_When_Out_Of_Scope_After_Creation)
{
	int dead_count = 0;
	{
		Ptr p = Ptr::make(dead_count);
		ASSERT_NE(nullptr, p.get());
		ASSERT_EQ(1, p->ref_);
	}
	ASSERT_EQ(1, dead_count);
}

TEST(RefCountPtr, To_Base_Increases_Ref_Count)
{
	int dead_count = 0;
	{
		Ptr ptr = Ptr::make(dead_count);
		ASSERT_EQ(1, ptr->ref_);
		{
			BasePtr base = ptr.to_base<RefCountedBase>();
			ASSERT_EQ(2, ptr->ref_);
			ASSERT_EQ(static_cast<RefCountedBase*>(ptr.get()), base.get());
		}
		ASSERT_EQ(1, ptr->ref_);
	}
	ASSERT_EQ(1, dead_count);
}

TEST(RefCountPtr, Move_Ctor_Transfers_Ptr_With_No_Ref_Count_Change)
{
	int dead_count = 0;
	{
		Ptr ptr = Ptr::make(dead_count);
		ASSERT_EQ(1, ptr->ref_);
		const RefCounted* raw = ptr.get();
		Ptr moved(std::move(ptr));
		ASSERT_EQ(nullptr, ptr.get());
		ASSERT_EQ(raw, moved.get());
		ASSERT_EQ(1, moved->ref_);
	}
	ASSERT_EQ(1, dead_count);
}

TEST(RefCountPtr, Move_Assignment_Transfers_Ptr_With_Ref_Count_Decrease_On_Left_Side)
{
	int dead_count_left = 0;
	int dead_count_right = 0;
	{
		Ptr left = Ptr::make(dead_count_left);
		Ptr right = Ptr::make(dead_count_right);
		const RefCounted* right_raw = right.get();
		ASSERT_EQ(1, left->ref_);
		ASSERT_EQ(1, right->ref_);
		left = std::move(right);
		ASSERT_EQ(1, dead_count_left);
		ASSERT_EQ(nullptr, right.get());
		ASSERT_EQ(right_raw, left.get());
		ASSERT_EQ(1, left->ref_);
	}
	ASSERT_EQ(1, dead_count_left);
	ASSERT_EQ(1, dead_count_right);
}

TEST(RefCountPtr, Copy_Ctor_Increases_Ref_Count_For_Both_Ptrs)
{
	int dead_count = 0;
	{
		Ptr ptr = Ptr::make(dead_count);
		ASSERT_EQ(1, ptr->ref_);
		const RefCounted* raw = ptr.get();
		Ptr copy(ptr);
		ASSERT_NE(nullptr, ptr.get());
		ASSERT_EQ(copy.get(), ptr.get());
		ASSERT_EQ(raw, copy.get());
		ASSERT_EQ(2, raw->ref_);
	}
	ASSERT_EQ(1, dead_count);
}

TEST(RefCountPtr, Copy_Assignment_Increases_Ref_Count_For_Both_Ptrs_With_Decrease_On_Left_Side)
{
	int dead_count_left = 0;
	int dead_count_right = 0;
	{
		Ptr left = Ptr::make(dead_count_left);
		Ptr left_copy(left);
		Ptr right = Ptr::make(dead_count_right);
		const RefCounted* right_raw = right.get();
		const RefCounted* left_raw = left.get();
		ASSERT_EQ(2, left->ref_);
		ASSERT_EQ(1, right->ref_);
		left = right;
		ASSERT_EQ(1, left_raw->ref_);
		ASSERT_EQ(right_raw, right.get());
		ASSERT_EQ(right_raw, left.get());
		ASSERT_EQ(2, left->ref_);
	}
	ASSERT_EQ(1, dead_count_left);
	ASSERT_EQ(1, dead_count_right);
}

TEST(RefCountPtr, Reset_Decrease_Ref_Count_And_Makes_Pointer_Null)
{
	int dead_count = 0;
	{
		Ptr ptr = Ptr::make(dead_count);
		ASSERT_EQ(1, ptr->ref_);
		Ptr copy = ptr;
		ASSERT_EQ(2, ptr->ref_);
		ASSERT_EQ(copy.get(), ptr.get());
		copy.reset();
		ASSERT_EQ(1, ptr->ref_);
		ASSERT_EQ(nullptr, copy.get());
	}
	ASSERT_EQ(1, dead_count);
}

TEST(RefCountPtr, Assign_To_Nullptr_Is_Reset)
{
	int dead_count = 0;
	{
		Ptr ptr = Ptr::make(dead_count);
		ASSERT_EQ(1, ptr->ref_);
		Ptr copy = ptr;
		ASSERT_EQ(2, ptr->ref_);
		ASSERT_EQ(copy.get(), ptr.get());
		copy = nullptr;
		ASSERT_EQ(1, ptr->ref_);
		ASSERT_EQ(nullptr, copy.get());
	}
	ASSERT_EQ(1, dead_count);
}
