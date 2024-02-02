#pragma once
#include <utility>
#include <boost/core/ref.hpp>
#include <boost/variant.hpp>
#include <varalgo/std_variant_traits.hpp>

namespace varalgo
{
	template <class ... VariantTypes>
	struct variant_traits<boost::variant<VariantTypes...>> : std::true_type
	{
		template <class Visitor, class ... Variants>
		inline static auto visit(Visitor && vis, Variants && ... vars)
		{
			return boost::apply_visitor(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
		}
	};
	
	template <class ... VariantTypes>
	struct variant_traits<boost::reference_wrapper<boost::variant<VariantTypes...>>> : std::true_type
	{
		template <class ... VarTypes>
		inline static constexpr auto rewrap(boost::reference_wrapper<boost::variant<VarTypes...>> var)
		{
			auto rewarpper = [](auto && val) -> boost::variant<boost::reference_wrapper<VarTypes>...>
				{ return boost::ref(val); };
			
			return boost::apply_visitor(rewarpper, var.get());
		}
		
		template <class Visitor, class ... Variants>
		inline static auto visit(Visitor && vis, Variants && ... vars)
		{
			return boost::apply_visitor(std::forward<Visitor>(vis), rewrap(vars)...);
		}
	};
	
	template <class ... VariantTypes>
	struct variant_traits<boost::reference_wrapper<const boost::variant<VariantTypes...>>> : std::true_type
	{
		template <class ... VarTypes>
		inline static constexpr auto rewrap(boost::reference_wrapper<const boost::variant<VarTypes...>> var)
		{
			auto rewarpper = [](auto && val) -> boost::variant<boost::reference_wrapper<const VarTypes>...>
				{ return boost::cref(val); };
			
			return boost::apply_visitor(rewarpper, var.get());
		}
		
		
		template <class Visitor, class ... Variants>
		inline static auto visit(Visitor && vis, Variants && ... vars)
		{
			return boost::apply_visitor(std::forward<Visitor>(vis), rewrap(vars)...);
		}
	};
}
