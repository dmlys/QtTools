#pragma once
#include <utility>
#include <boost/core/ref.hpp>
#include <boost/variant.hpp>
#include <varalgo/std_variant_traits.hpp>

namespace varalgo
{
	template <class Pred>
	struct variant_traits<boost::reference_wrapper<Pred>> : std::integral_constant<bool, variant_traits<Pred>::value>
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return variant_traits<Pred>::visit(std::forward<Visitor>(vis), vars.get()...);
		}
	};

	template <class Pred>
	struct variant_traits<boost::reference_wrapper<const Pred>> : std::integral_constant<bool, variant_traits<Pred>::value>
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return variant_traits<Pred>::visit(std::forward<Visitor>(vis), vars.get()...);
		}
	};

	template <class ... VariantTypes>
	struct variant_traits<boost::variant<VariantTypes...>> : std::true_type
	{
		template <class Visitor, class ... Variants>
		inline static auto visit(Visitor && vis, Variants && ... vars)
		{
			return boost::apply_visitor(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
		}
	};
}
