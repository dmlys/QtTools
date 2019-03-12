#pragma once
#include <variant>
#include <functional>
#include <boost/mp11.hpp>

//#include <boost/variant.hpp>


namespace viewed
{
	template <template <class ...> class Wrapper, class Func>
	struct wrap_functor_type;

	template <template <class ...> class Wrapper, class Func>
	using wrap_functor_type_t = typename wrap_functor_type<Wrapper, Func>::type;

	template <template <class ...> class Wrapper, class Func>
	auto wrap_functor(Func && func);


	template <template <class ...> class Wrapper, class Func>
	struct wrap_functor_type
	{
		using type = Wrapper<Func>;

		template <class Arg>
		static type create(Arg && arg) { return type(std::forward<Arg>(arg)); }
	};

	template <template <class ...> class Wrapper, class ... VariantTypes>
	struct wrap_functor_type<Wrapper, std::variant<VariantTypes...>>
	{
		using type = boost::mp11::mp_transform<Wrapper, std::variant<VariantTypes...>>;

		template <class Arg>
		static type create(Arg && arg)
		{
			// sadly std::variant does not have variant to variant converting constructor, must do by hand
			auto visitor = [](auto && arg) -> type
			{
				//using trait = wrap_functor_type<Wrapper, std::remove_reference_t<decltype(arg)>>;
				//return trait::create(std::forward<decltype(arg)>(arg));

				// clang have problems  handling this
				return wrap_functor<Wrapper>(std::forward<decltype(arg)>(arg));
			};

			return std::visit(visitor, std::forward<Arg>(arg));
		}
	};


	/// boost::variant support
	//template <template <class ...> class Wrapper, class ... VariantTypes>
	//struct wrap_functor_type<Wrapper, boost::variant<VariantTypes...>>
	//{
	//	using type = boost::mp11::mp_transform<Wrapper, boost::variant<VariantTypes...>>;
	//
	//	template <class Arg>
	//	static type create(Arg && arg)
	//	{
	//		//return type(std::forward<Arg>(arg));
	//
	//		 // sadly std::variant does not have variant to variant converting constructor, must do by hand
	//		 auto visitor = [](auto && arg) -> type
	//		 {
	//		 	using trait = wrap_functor_type<Wrapper, std::remove_reference_t<decltype(arg)>>;
	//		 	return trait::create(std::forward<decltype(arg)>(arg));
	//
	//		 	// clang have problems  handling this
	//		 	//return wrap_functor<Wrapper>(std::forward<decltype(arg)>(arg));
	//		 };
	//
	//		 return boost::apply_visitor(visitor, arg);
	//	}
	//};


	template <template <class ...> class Wrapper, class Func>
	struct wrap_functor_type<Wrapper, std::reference_wrapper<Func>>
	{
		template <class Arg> using make_reference_wrapper_t = decltype(std::ref(std::declval<Arg &>()));
		template <class Arg> using ref_wrapper = Wrapper<make_reference_wrapper_t<Arg>>;

		using base = wrap_functor_type<ref_wrapper, std::remove_cv_t<Func>>;
		using type = typename base::type;

		static type create(std::reference_wrapper<Func> ref) { return base::create(ref.get()); }
	};

	template <template <class ...> class Wrapper, class Func>
	struct wrap_functor_type<Wrapper, std::reference_wrapper<const Func>>
	{
		template <class Arg> using make_reference_wrapper_t = decltype(std::cref(std::declval<Arg &>()));
		template <class Arg> using ref_wrapper = Wrapper<make_reference_wrapper_t<Arg>>;

		using base = wrap_functor_type<ref_wrapper, std::remove_cv_t<Func>>;
		using type = typename base::type;

		static type create(std::reference_wrapper<const Func> ref) { return base::create(ref.get()); }
	};


	template <template <class ...> class Wrapper, class Func>
	auto wrap_functor(Func && func)
	{
		using func_type = std::remove_cv_t<std::remove_reference_t<Func>>;
		using trait = wrap_functor_type<Wrapper, func_type>;

		return trait::create(std::forward<Func>(func));
	}

}
