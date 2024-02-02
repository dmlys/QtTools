#pragma once
#include <utility>
#include <variant>
#include <functional>

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

	template <class ... Types>
	struct variant_traits<std::variant<Types...>> : std::true_type
	{
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return std::visit(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
		}
	};

	template <class ... Types>
	struct variant_traits<std::reference_wrapper<std::variant<Types...>>> : std::true_type
	{
		template <class ... VarTypes>
		inline static constexpr auto rewrap(std::reference_wrapper<std::variant<VarTypes...>> var)
		{
			auto rewarpper = [](auto && val) -> std::variant<std::reference_wrapper<VarTypes>...>
				{ return std::ref(val); };
			
			return std::visit(rewarpper, var.get());
		}
		
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return std::visit(std::forward<Visitor>(vis), rewrap(vars)...);
		}
	};
	
	template <class ... Types>
	struct variant_traits<std::reference_wrapper<const std::variant<Types...>>> : std::true_type
	{
		template <class ... VarTypes>
		inline static constexpr auto rewrap(std::reference_wrapper<const std::variant<VarTypes...>> var)
		{
			auto rewarpper = [](auto && val) -> std::variant<std::reference_wrapper<const VarTypes>...>
				{ return std::cref(val); };
			
			return std::visit(rewarpper, var.get());
		}
		
		template <class Visitor, class ... Variants>
		inline static constexpr auto visit(Visitor && vis, Variants && ... vars)
		{
			return std::visit(std::forward<Visitor>(vis), rewrap(vars)...);
		}
	};
}
