#pragma once
#include <rename_me/detail/config.h>

#include <type_traits>
#include <memory>

#include <cassert>

namespace nn
{
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

			explicit LazyStorageImpl()
				: T()
			{
			}

			LazyStorageImpl(LazyStorageImpl&&) = delete;
			LazyStorageImpl& operator=(LazyStorageImpl&&) = delete;
			LazyStorageImpl(const LazyStorageImpl&) = delete;
			LazyStorageImpl& operator=(const LazyStorageImpl&) = delete;

			template<typename... Args>
			void set_once(Args&&... args)
			{
				// move-assign
				get() = T(std::forward<Args>(args)...);
			}

			T& get()
			{
				return static_cast<T&>(*this);
			}

			bool has_value() const
			{
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

			LazyStorageImpl(LazyStorageImpl&&) = delete;
			LazyStorageImpl& operator=(LazyStorageImpl&&) = delete;
			LazyStorageImpl(const LazyStorageImpl&) = delete;
			LazyStorageImpl& operator=(const LazyStorageImpl&) = delete;

			~LazyStorageImpl()
			{
				if (set_)
				{
					get().~T();
				}
			}

			template<typename... Args>
			void set_once(Args&&... args)
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

		template<typename T, typename Tag = struct _>
		class LazyStorage : public LazyStorageImpl<T
			, IsLazyEboEnabled<T>::value>
		{
			using Base = LazyStorageImpl<T
				, IsLazyEboEnabled<T>::value>;
			using Base::Base;
		};

		template<typename Tag>
		class LazyStorage<void, Tag>
		{
		public:
			using is_ebo = std::true_type;

			explicit LazyStorage()
			{
			}

			LazyStorage(LazyStorage&&) = delete;
			LazyStorage& operator=(LazyStorage&&) = delete;
			LazyStorage(const LazyStorage&) = delete;
			LazyStorage& operator=(const LazyStorage&) = delete;

			void set_once()
			{
			}

			void get()
			{
			}

			bool has_value() const
			{
				return true;
			}
		};
	} // namespace detail
} // namespace nn
