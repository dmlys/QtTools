#pragma once
#include <variant>
#include <functional>
#include <boost/mp11.hpp>

//#include <boost/variant.hpp>


namespace viewed
{
	/************************************************************************/
	/*                   forward declarations                               */
	/************************************************************************/

	/// wrap_functor - wraps functor with given functor:
	///   wrap_functor<ext::indirect_functor>(somefunc) -> wrapped
	///   wrap_functor(somefunc, [](auto & pred, auto ... args) { return pred(args...); }) -> wrapped
	///
	/// supports std::variant and std::reference_wrapper
	///
	/// std::variant<std::greater<>, std::less<>> funcs;
	/// auto ref = std::cref(funcs);
	/// auto pred = wrap_functor<ext::indirect_functor>(ref);
	///
	/// varalgo::sort(..., pred);
	///

	template <template <class ...> class Wrapper, class Func>
	struct template_wrap_functor_type;

	template <template <class ...> class Wrapper, class Func>
	using template_wrap_functor_type_t = typename template_wrap_functor_type<Wrapper, Func>::type;


	template <class Wrapper, class Func, template <class ...> class ReferenceWrapper = boost::mp11::mp_identity_t>
	struct wrap_functor_type;

	template <class Wrapper, class Func, template <class ...> class ReferenceWrapper = boost::mp11::mp_identity_t>
	using wrap_functor_type_t = typename wrap_functor_type<Wrapper, Func>::type;


	template <template <class ...> class Wrapper, class Func>
	auto wrap_functor(Func && func);

	template <class Func, class Wrapper>
	auto wrap_functor(Func && func, Wrapper && wrapper);


	/************************************************************************/
	/*                   template_wrap implementation                       */
	/************************************************************************/

	template <template <class ...> class Wrapper, class Func>
	struct template_wrap_functor_type
	{
		using type = Wrapper<Func>;

		template <class Arg>
		static type create(Arg && arg) { return type(std::forward<Arg>(arg)); }
	};

	template <template <class ...> class Wrapper, class ... VariantTypes>
	struct template_wrap_functor_type<Wrapper, std::variant<VariantTypes...>>
	{
		using type = boost::mp11::mp_transform<Wrapper, std::variant<VariantTypes...>>;

		template <class Arg>
		static type create(Arg && arg)
		{
			// sadly std::variant does not have variant to variant converting constructor, must do by hand
			auto visitor = [](auto && arg) -> type
			{
				//using trait = template_wrap_functor_type<Wrapper, std::remove_reference_t<decltype(arg)>>;
				//return trait::create(std::forward<decltype(arg)>(arg));

				// clang have problems  handling this
				return wrap_functor<Wrapper>(std::forward<decltype(arg)>(arg));
			};

			return std::visit(visitor, std::forward<Arg>(arg));
		}
	};

	//// boost::variant support
	//template <template <class ...> class Wrapper, class ... VariantTypes>
	//struct template_wrap_functor_type<Wrapper, boost::variant<VariantTypes...>>
	//{
	//	using type = boost::mp11::mp_transform<Wrapper, boost::variant<VariantTypes...>>;
	//
	//	template <class Arg>
	//	static type create(Arg && arg)
	//	{
	//		auto visitor = [](auto && arg) -> type
	//		{
	//			using trait = template_wrap_functor_type<Wrapper, std::remove_reference_t<decltype(arg)>>;
	//			return trait::create(std::forward<decltype(arg)>(arg));
	//
	//			// clang have problems  handling this
	//			//return template_wrap_functor_type<Wrapper>(std::forward<decltype(arg)>(arg));
	//		};
	//
	//		return boost::apply_visitor(visitor, std::forward<Arg>(arg));
	//	}
	//};


	template <template <class ...> class Wrapper, class Func>
	struct template_wrap_functor_type<Wrapper, std::reference_wrapper<Func>>
	{
		template <class Arg> using make_reference_wrapper_t = decltype(std::ref(std::declval<Arg &>()));
		template <class Arg> using ref_wrapper = Wrapper<make_reference_wrapper_t<Arg>>;

		using base = template_wrap_functor_type<ref_wrapper, std::remove_cv_t<Func>>;
		using type = typename base::type;

		static type create(std::reference_wrapper<Func> ref) { return base::create(ref.get()); }
	};

	template <template <class ...> class Wrapper, class Func>
	struct template_wrap_functor_type<Wrapper, std::reference_wrapper<const Func>>
	{
		template <class Arg> using make_reference_wrapper_t = decltype(std::cref(std::declval<Arg &>()));
		template <class Arg> using ref_wrapper = Wrapper<make_reference_wrapper_t<Arg>>;

		using base = template_wrap_functor_type<ref_wrapper, std::remove_cv_t<Func>>;
		using type = typename base::type;

		static type create(std::reference_wrapper<const Func> ref) { return base::create(ref.get()); }
	};

	/************************************************************************/
	/*                   argument wrap implementation                       */
	/************************************************************************/

	template <class Wrapper, class Func>
	class wrapper_functor
	{
		Wrapper wrapper;
		Func func;

	public:
		template <class ... Args>
		decltype(auto) operator()(Args && ... args)
		{
			return wrapper(func, std::forward<Args>(args)...);
		}

		template <class ... Args>
		decltype(auto) operator()(Args && ... args) const
		{
			return wrapper(func, std::forward<Args>(args)...);
		}

		wrapper_functor(Wrapper wrapper, Func func)
		    : wrapper(std::move(wrapper)), func(std::move(func)) {}
	};

	template <class Wrapper, class Func, template <class ...> class ReferenceWrapper>
	struct wrap_functor_type
	{
		using type = wrapper_functor<Wrapper, ReferenceWrapper<Func>>;

		template <class ... Args>
		static type create(Args && ... args) { return type(std::forward<Args>(args)...); }
	};

	template <class Wrapper, class ... VariantTypes, template <class ...> class ReferenceWrapper>
	struct wrap_functor_type<Wrapper, std::variant<VariantTypes...>, ReferenceWrapper>
	{
		using type = std::variant<wrapper_functor<Wrapper, ReferenceWrapper<VariantTypes>>...>;

		template <class WrapperArg, class Arg>
		static type create(WrapperArg && wrapper, Arg && arg)
		{
			// sadly std::variant does not have variant to variant converting constructor, must do by hand
			auto visitor = [&wrapper](auto && arg) -> type
			{
				using arg_type = std::remove_reference_t<decltype(arg)>;
				using ref_type = ReferenceWrapper<arg_type>;

				if constexpr (std::is_same_v<arg_type, ref_type>)
					return wrap_functor(std::forward<decltype(arg)>(arg), std::forward<WrapperArg>(wrapper));
				else
					return wrap_functor(ref_type(std::forward<decltype(arg)>(arg)), std::forward<WrapperArg>(wrapper));
			};

			return std::visit(visitor, std::forward<Arg>(arg));
		}
	};

	//// boost::variant support
	//template <class Wrapper, class ... VariantTypes, template <class ...> class ReferenceWrapper>
	//struct wrap_functor_type<Wrapper, boost::variant<VariantTypes...>, ReferenceWrapper>
	//{
	//	using type = boost::variant<wrapper_functor<Wrapper, ReferenceWrapper<VariantTypes>>...>;
	//
	//	template <class WrapperArg, class Arg>
	//	static type create(WrapperArg && wrapper, Arg && arg)
	//	{
	//		auto visitor = [&wrapper](auto && arg) -> type
	//		{
	//			using arg_type = std::remove_reference_t<decltype(arg)>;
	//			using ref_type = ReferenceWrapper<arg_type>;
	//
	//			if constexpr (std::is_same_v<arg_type, ref_type>)
	//				return wrap_functor(std::forward<decltype(arg)>(arg), std::forward<WrapperArg>(wrapper));
	//			else
	//				return wrap_functor(ref_type(std::forward<decltype(arg)>(arg)), std::forward<WrapperArg>(wrapper));
	//		};
	//
	//		return boost::apply_visitor(visitor, std::forward<Arg>(arg));
	//	}
	//};

	template <class Wrapper, class Func, template <class ...> class ReferenceWrapper>
	struct wrap_functor_type<Wrapper, std::reference_wrapper<Func>, ReferenceWrapper>
	{
		template <class Arg> using make_reference_t = decltype(std::ref(std::declval<Arg &>()));
		using base = wrap_functor_type<Wrapper, Func, make_reference_t>;
		using type = typename base::type;

		static type create(Wrapper && wrapper, std::reference_wrapper<Func> ref) { return base::create(std::forward<Wrapper>(wrapper), ref.get()); }
	};

	template <class Wrapper, class Func, template <class ...> class ReferenceWrapper>
	struct wrap_functor_type<Wrapper, std::reference_wrapper<const Func>, ReferenceWrapper>
	{
		template <class Arg> using make_reference_t = decltype(std::cref(std::declval<Arg &>()));
		using base = wrap_functor_type<Wrapper, Func, make_reference_t>;
		using type = typename base::type;

		static type create(Wrapper && wrapper, std::reference_wrapper<const Func> ref) { return base::create(std::forward<Wrapper>(wrapper), ref.get()); }
	};





	template <template <class ...> class Wrapper, class Func>
	auto wrap_functor(Func && func)
	{
		using func_type = std::remove_cv_t<std::remove_reference_t<Func>>;
		using trait = template_wrap_functor_type<Wrapper, func_type>;

		return trait::create(std::forward<Func>(func));
	}

	template <class Func, class Wrapper>
	auto wrap_functor(Func && func, Wrapper && wrapper)
	{
		using func_type = std::remove_cv_t<std::remove_reference_t<Func>>;
		using trait = wrap_functor_type<Wrapper, func_type>;

		return trait::create(std::forward<Wrapper>(wrapper), std::forward<Func>(func));
	}

}
