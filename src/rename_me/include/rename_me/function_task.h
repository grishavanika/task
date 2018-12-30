#pragma once
#include <rename_me/task.h>
#include <rename_me/storage.h>
#include <rename_me/detail/cpp_20.h>

#include <functional>
#include <tuple>
#include <variant>

#include <cassert>
#include <cstdint>

namespace nn
{
	namespace detail
	{
		template<typename R>
		struct FunctionTaskReturnImpl
		{
			using is_expected = std::bool_constant<false>;
			using is_task = std::bool_constant<false>;
			using type = Task<R, void>;
			using custom_task = ICustomTask<R, void>;
			using expected_type = expected<R, void>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<expected<T, E>>
		{
			using is_expected = std::bool_constant<true>;
			using is_task = std::bool_constant<false>;
			using type = Task<T, E>;
			using custom_task = ICustomTask<T, E>;
			using expected_type = expected<T, E>;
		};

		template<typename T, typename E>
		struct FunctionTaskReturnImpl<Task<T, E>>
		{
			using is_expected = std::bool_constant<false>;
			using is_task = std::bool_constant<true>;
			using type = Task<T, E>;
			using custom_task = ICustomTask<T, E>;
			using expected_type = expected<T, E>;
		};

		template<typename F, typename ArgsTuple
			, typename R = remove_cvref_t<decltype(
				std::apply(std::declval<F>(), std::declval<ArgsTuple>()))>>
		struct FunctionTaskReturn : FunctionTaskReturnImpl<R>
		{
		};

		template<typename Return // FunctionTaskReturn helper
			, typename CustomTask // Proper ICustomTask<T, E>
			, typename F
			, typename ArgsTuple>
		class FunctionTask;

		template<typename Return
			, typename CustomTask
			, typename F
			, typename... Args>
		class FunctionTask<Return, CustomTask, F, std::tuple<Args...>> final
			: public CustomTask
			, private F // Enable EBO if possible
			, private std::tuple<Args...>
		{
		public:
			enum class State : std::uint8_t
			{
				None,
				Invoked,
				Canceled,
			};
			
			using Value = std::variant<
				typename Return::type // Either Task<> or
				, typename Return::expected_type>; // expected<>
			using ArgsBase = std::tuple<Args...>;
			using IsExpected = typename Return::is_expected;
			using IsTask = typename Return::is_task;
			using IsValueVoid = std::is_same<
				typename Return::type::value_type, void>;
			using IsApplyVoid = std::integral_constant<bool
				, !IsExpected::value && IsValueVoid::value>;
			// Workaround for "broken"/temporary expected<void, void> we have now 
			using IsVoidExpected = std::integral_constant<bool
				, IsExpected::value && IsValueVoid::value>;

			explicit FunctionTask(F&& f, std::tuple<Args...>&& args)
				: F(std::move(f))
				, ArgsBase(std::move(args))
				, value_()
				, state_(State::None)
				, value_set_(false)
			{
			}

			virtual ~FunctionTask() override
			{
				if (value_set_)
				{
					value().~Value();
				}
			}

			FunctionTask(FunctionTask&&) = delete;
			FunctionTask& operator=(FunctionTask&&) = delete;
			FunctionTask(const FunctionTask&) = delete;
			FunctionTask& operator=(const FunctionTask&) = delete;

			virtual Status tick() override
			{
				if (IsTask() && (state_ == State::Invoked))
				{
					return get_task().status();
				}
				if (state_ != State::None)
				{
					return Status::Failed;
				}

				state_ = State::Invoked;
				call_impl(IsApplyVoid());
				if (IsExpected())
				{
					return get_expected().has_value()
						? Status::Successful
						: Status::Failed;
				}
				else if (IsTask())
				{
					return get_task().status();
				}
				return Status::Successful;
			}

			typename Return::type& get_task()
			{
				assert(value_set_);
				auto& v = value();
				assert(v.index() == 0);
				return std::get<0>(v);
			}

			typename Return::expected_type& get_expected()
			{
				assert(value_set_);
				auto& v = value();
				assert(v.index() == 1);
				return std::get<1>(v);
			}

			virtual bool cancel() override
			{
				if (state_ == State::None)
				{
					// Canceled before tick()
					state_ = State::Canceled;
					return true;
				}
				return false;
			}

			virtual typename Return::expected_type& get() override
			{
				return (IsTask() ? get_task().get() : get_expected());
			}

			void call_impl(std::true_type/*f(...) is void*/)
			{
				using Expected = typename Return::expected_type;
				new(std::addressof(value_)) Value(Expected());
				value_set_ = true;
				assert(!IsTask()
					&& "void f() should construct expected<void, ...> return");
				std::apply(std::move(f()), std::move(args()));
			}

			void call_impl(std::false_type/*f(...) is NOT void*/)
			{
				// f() returns Task<> or single value T or expected<>.
				// In any case std::variant<> c-tor will resolve to proper active
				// member since Task<> can't be constructed from T or expected.
				new(std::addressof(value_)) Value(
					std::apply(std::move(f()), std::move(args())));
				value_set_ = true;
				assert((!IsTask() && (value().index() == 1))
					|| ( IsTask() && (value().index() == 0)));
			}

			F& f()
			{
				return static_cast<F&>(*this);
			}

			std::tuple<Args...>& args()
			{
				return static_cast<std::tuple<Args...>&>(*this);
			}

			Value& value()
			{
				return *reinterpret_cast<Value*>(std::addressof(value_));
			}

		private:
			using ValueStorage = typename std::aligned_storage<
				sizeof(Value), alignof(Value)>::type;

			ValueStorage value_;
			State state_;
			bool value_set_;
		};

	} // namespace detail

	// #TODO: probably, if function returns T&, reference should not be discarded.
	// Looks like it depends on std::expected: if it supports references - they can 
	// be added easily.
	template<typename F, typename... Args>
	typename detail::FunctionTaskReturn<F
		, std::tuple<detail::remove_cvref_t<Args>...>>::type
			make_task(Scheduler& scheduler, F&& f, Args&&... args)
	{
		using Function = detail::remove_cvref_t<F>;
		using ArgsTuple = std::tuple<detail::remove_cvref_t<Args>...>;
		using FunctionTaskReturn = detail::FunctionTaskReturn<F, ArgsTuple>;
		using ReturnTask = typename FunctionTaskReturn::type;

		using Impl = detail::FunctionTask<
			FunctionTaskReturn
			, typename FunctionTaskReturn::custom_task
			, Function
			, ArgsTuple>;
		
		auto task = std::make_unique<Impl>(std::forward<F>(f)
			, ArgsTuple(std::forward<Args>(args)...));

		return ReturnTask(scheduler, std::move(task));
	}

} // namespace nn
