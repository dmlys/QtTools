#pragma once
#include <utility>
#include <variant>

namespace varalgo
{
	template <class Pred>
	struct variant_traits : std::false_type
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return std::forward<Visitor>(vis)(std::forward<Variants>(vars)...);
		}
	};

	template <class Pred>
	struct variant_traits<std::reference_wrapper<Pred>> : std::integral_constant<bool, variant_traits<Pred>::value>
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return variant_traits<Pred>::visit(std::forward<Visitor>(vis), vars.get()...);
		}
	};

	template <class Pred>
	struct variant_traits<std::reference_wrapper<const Pred>> : std::integral_constant<bool, variant_traits<Pred>::value>
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return variant_traits<Pred>::visit(std::forward<Visitor>(vis), vars.get()...);
		}
	};

	template <class ... Types>
	struct variant_traits<std::variant<Types...>> : std::true_type
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return std::visit(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
		}
	};
}
