#pragma once
#include <utility>

#include <cstddef>
#include <cassert>

#if defined(__GNUC__) && !defined(__clang__)
#  define NN_GCC_COMPILER 1
#else
#  define NN_GCC_COMPILER 0
#endif

#if (NN_GCC_COMPILER)
// "this" not being allowed in noexcept clauses
// See bug-report: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52869
#  define NN_NOEXCEPT(C)
#else
#  define NN_NOEXCEPT(C) noexcept(C)
#endif

namespace nn
{
	namespace detail
	{
		// `T` must satisfy type with:
		// (1) bool remove_ref_count() noexcept
		// (2) void add_ref_count() noexcept
		template<typename T>
		class RefCountPtr
		{
			template<typename U>
			friend class RefCountPtr;
		public:
			using type = T;

			template<typename... Args>
			static RefCountPtr make(Args&&... args)
			{
				return RefCountPtr(new T(std::forward<Args>(args)...));
			}

			RefCountPtr(RefCountPtr&& rhs) NN_NOEXCEPT(true)
				: ptr_(rhs.ptr_)
			{
				rhs.ptr_ = nullptr;
			}

			RefCountPtr& operator=(std::nullptr_t) NN_NOEXCEPT(NN_NOEXCEPT(reset()))
			{
				reset();
				return *this;
			}

			RefCountPtr& operator=(RefCountPtr&& rhs) NN_NOEXCEPT(NN_NOEXCEPT(reset()))
			{
				if (this != &rhs)
				{
					reset();
					std::swap(ptr_, rhs.ptr_);
				}
				return *this;
			}

			RefCountPtr(const RefCountPtr& rhs) NN_NOEXCEPT(NN_NOEXCEPT(acquire()))
				: ptr_(rhs.ptr_)
			{
				acquire();
			}

			RefCountPtr& operator=(const RefCountPtr& rhs)
				NN_NOEXCEPT(NN_NOEXCEPT(reset()) && NN_NOEXCEPT(acquire()))
			{
				if (this != &rhs)
				{
					reset();
					ptr_ = rhs.ptr_;
					acquire();
				}
				return *this;
			}

			// I'm lazy, templated c-tor should exists
			template<typename Base>
			RefCountPtr<Base> to_base()
			{
				static_assert(std::is_base_of<Base, T>::value
					, "Base should be base class of T");
				acquire();
				return RefCountPtr<Base>(ptr_);
			}

			void reset() NN_NOEXCEPT(NN_NOEXCEPT(release()))
			{
				release();
				ptr_ = nullptr;
			}

			explicit operator bool() const
			{
				return (ptr_ != nullptr);
			}

			T* get()
			{
				return ptr_;
			}

			T* operator->()
			{
				return ptr_;
			}

			T* operator->() const
			{
				return ptr_;
			}

			T& operator*()
			{
				assert(ptr_);
				return *ptr_;
			}

			~RefCountPtr()
			{
				release();
			}

		private:
			explicit RefCountPtr(T* ptr)
				: ptr_(ptr)
			{
				assert(ptr_);
			}

			void destroy() NN_NOEXCEPT(NN_NOEXCEPT(delete ptr_))
			{
				delete ptr_;
				ptr_ = nullptr;
			}

			void acquire() NN_NOEXCEPT(true)
			{
				if (ptr_)
				{
					ptr_->add_ref_count();
				}
			}

			void release()
				NN_NOEXCEPT(NN_NOEXCEPT(ptr_->remove_ref_count()) && NN_NOEXCEPT(destroy()))
			{
				if (ptr_)
				{
					if (const bool end = ptr_->remove_ref_count())
					{
						destroy();
					}
				}
			}

		private:
			T* ptr_;
		};
	} // namespace detail
} // namespace nn

#undef NN_NOEXCEPT
#undef NN_GCC_COMPILER
