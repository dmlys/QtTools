#pragma once
#include <ext/functors/indirect_functor.hpp>
#include <viewed/wrap_functor.hpp>

namespace viewed
{
	/// преобразует предикат в ext::indirect_functor<Pred>
	template <class Pred>
	using make_indirect_functor_type_t = viewed::wrap_functor_type_t<ext::indirect_functor, Pred>;

	template <class Pred>
	inline auto make_indirect_functor(const Pred & pred)
	{
		return viewed::wrap_functor<ext::indirect_functor>(pred);
	}
}
