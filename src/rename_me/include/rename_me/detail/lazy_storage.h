#pragma once
#include <rename_me/detail/config.h>

#include <type_traits>
#include <memory>

#include <cassert>

namespace nn
{
	// LazyStorage<T> represents type that can be default-constructed
	// even if T is not (like optional<T>).
	// If possible, EBO used hence if is_empty<T> then is_empty<LazyStorage<T>> too.
	// There are few limitations however: for EBO-enabled storage
	// T will be default-constructed once and move-assigned when emplace
	// on LazyStorage<> called.
	// 
	// LazyStorage<> is not copy/move-enabled for simplicity purpose

	namespace detail
	{

		template<typename T>
		struct IsLazyEboEnabled : std::integral_constant<bool
			, std::is_empty<T>::value
			&& !std::is_final<T>::value
			// Specific bit for our Storage purpose:
			// we require storage to be default-constructible.
			&& std::is_default_constructible<T>::value
			// And, as result, is should be move-assignable to
			// put some value later
			&& std::is_move_assignable<T>::value>
		{
		};

		template<typename T, bool IsEbo>
		class LazyStorageImpl;

		template<typename T>
		class NN_EBO_CLASS LazyStorageImpl<T, true/*IsEbo*/> : T
		{
		public:
			using is_ebo = std::true_type;

			template<typename... Args>
			void emplace_once(Args&&... args)
			{
				// Move assign (was default constructed)
				get() = T(std::forward<Args>(args)...);
			}

			T& get()
			{
				return static_cast<T&>(*this);
			}

			bool has_value() const
			{
				// Always has value since default constructed
				return true;
			}
		};

		template<typename T>
		class LazyStorageImpl<T, false/*IsEbo*/>
		{
			using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
		public:
			using is_ebo = std::false_type;

			explicit LazyStorageImpl()
				: data_()
				, set_(false)
			{
			}

			~LazyStorageImpl()
			{
				if (set_)
				{
					get().~T();
				}
			}

			template<typename... Args>
			void emplace_once(Args&&... args)
			{
				assert(!set_);
				new(ptr()) T(std::forward<Args>(args)...);
				set_ = true;
			}

			T& get()
			{
				assert(set_);
				return *static_cast<T*>(ptr());
			}

			bool has_value() const
			{
				return set_;
			}

		private:
			void* ptr()
			{
				return std::addressof(data_);
			}

		private:
			Storage data_;
			bool set_;
		};

		template<>
		class LazyStorageImpl<void, !IsLazyEboEnabled<void>::value>
		{
		public:
			using is_ebo = std::true_type;
			explicit LazyStorageImpl() = default;
			void emplace_once() { }
			void get() { }
			bool has_value() const { return true; }
		};

		// Some unique Tag can be specified in case LazyStorage<>
		// is used multiple time in single hierarchy with same T.
		template<typename T, typename Tag = struct _>
		class LazyStorage : private LazyStorageImpl<T, IsLazyEboEnabled<T>::value>
		{
			using Storage = LazyStorageImpl<T, IsLazyEboEnabled<T>::value>;
		public:
			explicit LazyStorage() = default;

			using is_ebo = typename Storage::is_ebo;
			using Storage::emplace_once;
			using Storage::get;
			using Storage::has_value;

			LazyStorage(LazyStorage&&) = delete;
			LazyStorage& operator=(LazyStorage&&) = delete;
			LazyStorage(const LazyStorage&) = delete;
			LazyStorage& operator=(const LazyStorage&) = delete;
		};

	} // namespace detail
} // namespace nn
