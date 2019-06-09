#pragma once
#include <tl/expected.hpp>
#include <optional>

#include <type_traits>

namespace nn
{

	template<typename>
	struct is_expected;

	// #TODO: make consistent interfcace.
	// Behavior: expected<void, void> (or any combination of void and T)
	// should be valid with well-defined behavior.
	template<typename T, typename E>
	struct expected : tl::expected<T, E>
	{
		using Base = tl::expected<T, E>;
		using Base::Base;
	};

	template<typename E>
	using unexpected = tl::unexpected<E>;

	struct unexpected_void {};

	template<typename T>
	struct expected<T, void> : std::optional<T>
	{
		using Base = std::optional<T>;
		using Base::Base;
	};

	template<>
	struct expected<void, void>
	{
		expected()
			: ok_(true)
		{
		}

		expected(unexpected_void)
			: ok_(false)
		{
		}

		bool has_value() const
		{
			return ok_;
		}

	private:
		bool ok_;
	};

	template<typename>
	struct is_expected : std::false_type { };

	template<typename T, typename E>
	struct is_expected<expected<T, E>> : std::true_type { };

	namespace detail
	{
		template<typename E>
		struct UnexpectedBuilder;

		template<typename T, typename E>
		struct UnexpectedBuilder<expected<T, E>>
		{
			// ... and move-constructible to be precice
			// because of how we use expected<...>.
			// Will not be needed if expected can construct Error in place
			static_assert(std::is_default_constructible_v<E>
				, "We need to create error when unexpected situation "
				"happens (like Cancel). Error should be default "
				"constructible specifically for this case and only");
			static expected<T, E> make()
			{
				return expected<T, E>(unexpected<E>(E()));
			}
		};

		template<typename T>
		struct UnexpectedBuilder<expected<T, void>>
		{
			static expected<T, void> make()
			{
				// Constructs optional<T> with has_value() == false
				return expected<T, void>();
			}
		};

		template<>
		struct UnexpectedBuilder<expected<void, void>>
		{
			static expected<void, void> make()
			{
				// And yet another special case. Basically, it's a bool
				return expected<void, void>(unexpected_void());
			}
		};
	} // namespace detail

	template<typename E>
	E MakeExpectedWithDefaultError()
	{
		static_assert(is_expected<E>(), "Expecting the expected<>");
		return detail::UnexpectedBuilder<E>::make();
	}

	template<typename Expected, typename Error>
	Expected MakeExpectedWithError(Error&& value)
	{
		static_assert(is_expected<Expected>(), "Expecting the expected<>");
		return Expected(unexpected<std::remove_reference_t<Error>>(std::move(value)));
	}

	template<typename T, typename E>
	void SetExpectedWithError(expected<T, E>& data, E value)
	{
		data = expected<T, E>(unexpected<E>(std::move(value)));
	}

	template<typename T>
	void SetExpectedWithError(expected<T, void>& data)
	{
		data = detail::UnexpectedBuilder<expected<T, void>>::make();
	}
	
	inline void SetExpectedWithError(expected<void, void>& data)
	{
		data = detail::UnexpectedBuilder<expected<void, void>>::make();
	}

} // namespace nn
