#pragma once
#include <utility>     // for std::get
#include <tuple>       // for std::get
#include <viewed/wrap_functor.hpp>

namespace viewed
{
	/// functor which transforms it's arguments via get<Index> passes them to wrapped functor
	template <std::size_t Index, class Functor>
	struct get_functor
	{
		Functor func;

		template <class ... Args>
		decltype(auto) operator()(Args && ... args) const
		{
			using std::get;
			return func(get<Index>(std::forward<Args>(args))...);
		}

		get_functor() = default;
		get_functor(Functor func) : func(std::move(func)) {}
	};

	template <std::size_t Index, class Functor>
	struct make_get_functor_type
	{
		template <class Arg>
		using helper = get_functor<Index, Arg>;

		using type = viewed::wrap_functor_type_t<helper, Functor>;
	};

	template <std::size_t Index, class Functor>
	using make_get_functor_type_t = typename make_get_functor_type<Index, Functor>::type;

	template <std::size_t Index, class Functor>
	inline auto make_get_functor(const Functor & func)
	{
		return viewed::wrap_functor<make_get_functor_type<Index, Functor>::template helper>(func);
	}
}
