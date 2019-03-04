#pragma once
#include <cstddef>
#include <memory>  // for std::unique_ptr
#include <utility> // for std::exchange, std::in_place_index, std::in_place_type
#include <type_traits>
#include <ext/type_traits.hpp>

#include <boost/integer/static_log2.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/bind.hpp>

namespace viewed
{
	/// pointer_variant<int *, type *, ...>
	/// variant class for pointer types(only pointer types allowed).
	/// sizeof(pointer_variant<...>) == sizeof(std::uintptr_t) always,
	/// current type is held in lowest bits of pointer value.
	/// 
	/// Only head allocated pointers or pointers with big enough alignment are allowed.
	/// (only runtime assert checks are made, so you should be careful)
	template <class ... PointerTypes>
	class pointer_variant;

	/// thrown by get method
	class bad_variant_access : public std::exception
	{
	public:
		const char * what() const noexcept { return "bad_variant_access"; }
	};

	/************************************************************************/
	/*                      pointer_variant_size                            */
	/************************************************************************/
	template <class Type>
	struct pointer_variant_size; /* undefined */
		
	template <class Type> struct pointer_variant_size<         const Type> : pointer_variant<Type>::type {};
	template <class Type> struct pointer_variant_size<      volatile Type> : pointer_variant<Type>::type {};
	template <class Type> struct pointer_variant_size<const volatile Type> : pointer_variant<Type>::type {};

	template <class... PointerTypes>
	struct pointer_variant_size<pointer_variant<PointerTypes...>>
		: std::integral_constant<std::size_t, sizeof...(PointerTypes)> {};

	template <class... PointerTypes>
	constexpr auto pointer_variant_size_v = pointer_variant_size<PointerTypes...>::value;


	template <class Type>
	struct is_pointer_variant : std::false_type {};

	template <class ... Types>
	struct is_pointer_variant<pointer_variant<Types...>> : std::true_type {};

	template <class Type>
	constexpr bool is_pointer_variant_v = is_pointer_variant<Type>::value;

	/************************************************************************/
	/*                   pointer_variant_alternative                        */
	/************************************************************************/
	template <std::size_t I, class Type>
	struct pointer_variant_alternative; // undefined

	template <size_t I, class Type>
	struct pointer_variant_alternative<I, const Type>
	{
		using type = std::add_const_t<typename pointer_variant_alternative<I, Type>::type>;
	};

	template <size_t I, class Type>
	struct pointer_variant_alternative<I, volatile Type>
	{
		using type = std::add_volatile_t<typename pointer_variant_alternative<I, Type>::type>;
	};

	template <size_t I, class Type>
	struct pointer_variant_alternative<I, const volatile Type>
	{
		using type = std::add_cv_t<typename pointer_variant_alternative<I, Type>::type>;
	};

	template <size_t I, class... PointerTypes>
	struct pointer_variant_alternative<I, pointer_variant<PointerTypes...>>
	{
		static_assert(I < sizeof...(PointerTypes), "index out of bounds");
		using type = boost::mp11::mp_at_c<pointer_variant<PointerTypes...>, I>;
	};

	template <size_t I, class Type>
	using pointer_variant_alternative_t = typename pointer_variant_alternative<I, Type>::type;

	/************************************************************************/
	/*                   forward declarations                               */
	/************************************************************************/
	template <class T, class ... PointerTypes>
	constexpr bool holds_alternative(const pointer_variant<PointerTypes...> & v) noexcept;

	template <class T, class ... Types>
	constexpr T get(const pointer_variant<Types...> & v);

	template <std::size_t Index, class ... Types>
	constexpr auto get(const pointer_variant<Types...> & v) ->
		pointer_variant_alternative_t<Index, pointer_variant<Types...>>;

	template <class Visitor, class ... Variants>
	constexpr auto visit(Visitor && vis, Variants && ... vars) ->
		std::enable_if_t<
			std::conjunction_v<is_pointer_variant<ext::remove_cvref_t<Variants>>...>,
			std::invoke_result_t<Visitor, boost::mp11::mp_first<ext::remove_cvref_t<Variants>>...>
		>;

	template <class ResultType, class Visitor, class ... Variants>
	constexpr auto visit(Visitor && vis, Variants && ... vars) ->
		std::enable_if_t<
			std::conjunction_v<is_pointer_variant<ext::remove_cvref_t<Variants>>...>,
			ResultType
		>;

	/************************************************************************/
	/*                    pointer_variant helpers                           */
	/************************************************************************/
	namespace pointer_variant_detail
	{
		template <std::size_t Num, class Type>
		struct overload_member_impl
		{
			Type operator()(Type) const;
		};

		template <class ... Types>
		struct overload_set_impl;

		template <std::size_t ... Indexes, class ... Types>
		struct overload_set_impl<std::index_sequence<Indexes...>, boost::mp11::mp_list<Types...>> : overload_member_impl<Indexes, Types>...
		{
			using overload_member_impl<Indexes, Types>::operator()...;
		};

		template <class ... Types>
		struct overload_set : overload_set_impl<std::index_sequence_for<Types...>, boost::mp11::mp_list<Types...>>
		{
			using base_type = overload_set_impl<std::index_sequence_for<Types...>, boost::mp11::mp_list<Types...>>;
			using base_type::operator();
		};


		template <class TypeList, class Type, class = void>
		struct invokable_impl : std::false_type {};

		template <template <class...> class TypeList, class ... Types, class Type>
		struct invokable_impl<TypeList<Types...>, Type, std::void_t<std::invoke_result_t<overload_set<Types...>, Type>>> : std::true_type {};

		template <class TypeList, class Type>
		using invokable = invokable_impl<TypeList, Type>;

		template <class TypeList, class Type>
		const auto invokable_v = invokable<TypeList, Type>::value;


		template <class WithTypeList, class ArgumentTypeList>
		using all_invokable = boost::mp11::mp_all_of_q<ArgumentTypeList, boost::mp11::mp_bind<invokable, WithTypeList, boost::mp11::_1>>;

		template <class WithTypeList, class ArgumentTypeList>
		const auto all_invokable_v = all_invokable<WithTypeList, ArgumentTypeList>::value;


		template <class list, class ptr_type>
		constexpr auto find_v = boost::mp11::mp_find_if_q<list, boost::mp11::mp_bind<std::is_same, ptr_type, boost::mp11::_1>>::value;

		template <class list, class ptr_type>
		constexpr auto extactly_once = boost::mp11::mp_count<list, ptr_type>::value == 1;
	}

	/************************************************************************/
	/*                    pointer_variant                                   */
	/************************************************************************/
	template <class ... PointerTypes>
	class pointer_variant
	{
		template <class ... OtherPointerTypes> friend class pointer_variant;

		using self_type = pointer_variant;

		static constexpr unsigned TYPE_BITS = boost::static_log2<sizeof...(PointerTypes)>::value + (sizeof...(PointerTypes) % 2 != 0);
		static constexpr unsigned PTR_OFFSET = TYPE_BITS + 1;
		static constexpr std::uintptr_t PTR_MASK = ~std::uintptr_t(0) >> (sizeof(std::uintptr_t) * CHAR_BIT - PTR_OFFSET);

		static_assert(PTR_OFFSET <= boost::static_log2<sizeof(std::max_align_t)>::value,
			"not enough align bytes to store pointer type");

		static_assert(boost::mp11::mp_all_of<self_type, std::is_pointer>::value,
			"only pointer types allowed");

	private:
		union 
		{
			struct
			{
				std::uintptr_t m_owning : 1;
				std::uintptr_t m_type   : TYPE_BITS;
				std::uintptr_t m_ptr    : sizeof(std::uintptr_t) * CHAR_BIT - PTR_OFFSET;
			};

			std::uintptr_t m_val;
		};

	private:
		static constexpr std::uintptr_t pack(const void * extptr) { return reinterpret_cast<std::uintptr_t>(extptr) >> PTR_OFFSET; }
		static constexpr void * unpack(std::uintptr_t extptr)     { return reinterpret_cast<void *>(extptr << PTR_OFFSET); }

	private:
		void destroy() noexcept;

	public:
		//constexpr void * release()       noexcept { auto ptr = unpack(m_ptr); m_val = 0; return ptr; }
		constexpr void * pointer() const noexcept { return unpack(m_ptr); }
		constexpr std::size_t index() const noexcept { return m_type; }
		void swap(pointer_variant & other) noexcept { std::swap(m_val, other.m_val); }

	public:
		template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> = 0>
		pointer_variant(Type * ptr) noexcept;

		template <class T, class Type, std::enable_if_t<pointer_variant_detail::extactly_once<pointer_variant<PointerTypes...>, T>, int> = 0>
		explicit pointer_variant(std::in_place_type_t<T>, Type * ptr) noexcept;

		template <std::size_t Index, class Type, std::enable_if_t<std::is_convertible_v<Type *, pointer_variant_alternative_t<Index, pointer_variant<PointerTypes...>>>, int> = 0>
		explicit pointer_variant(std::in_place_index_t<Index>, Type * ptr) noexcept;

		template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> = 0>
		pointer_variant(std::unique_ptr<Type> ptr) noexcept;

		template <class T, class Type, std::enable_if_t<pointer_variant_detail::extactly_once<pointer_variant<PointerTypes...>, T>, int> = 0>
		explicit pointer_variant(std::in_place_type_t<T>, std::unique_ptr<Type> ptr) noexcept;

		template <std::size_t Index, class Type, std::enable_if_t<std::is_convertible_v<Type *, pointer_variant_alternative_t<Index, pointer_variant<PointerTypes...>>>, int> = 0>
		explicit pointer_variant(std::in_place_index_t<Index>, std::unique_ptr<Type> ptr) noexcept;

		template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> = 0>
		pointer_variant & operator =(Type * ptr) noexcept;

		template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> = 0>
		pointer_variant & operator =(std::unique_ptr<Type> ptr) noexcept;

		// conversion constructor from variant of different pointer types
		template <class ... OtherPointerTypes, std::enable_if_t<pointer_variant_detail::all_invokable_v<pointer_variant<PointerTypes...>, pointer_variant<OtherPointerTypes...>>, int> = 0>
		pointer_variant(pointer_variant<OtherPointerTypes...> && other) noexcept;

	public:
		pointer_variant(pointer_variant &&) noexcept;
		pointer_variant & operator =(pointer_variant &&) noexcept;

		pointer_variant(const pointer_variant &) = delete;
		pointer_variant & operator =(const pointer_variant &) = delete;

		pointer_variant() noexcept;
		~pointer_variant() noexcept;
	};

	/************************************************************************/
	/*            pointer_variant implementation                            */
	/************************************************************************/
	template <class ... Types>
	inline void swap(pointer_variant<Types...> & v1, pointer_variant<Types...> & v2)
	{
		v1.swap(v2);
	}

	template <class ... PointerTypes>
	template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> /*= 0*/>
	inline pointer_variant<PointerTypes...>::pointer_variant(Type * ptr) noexcept
	{
		using result_type = std::invoke_result_t<pointer_variant_detail::overload_set<PointerTypes...>, Type *>;
		constexpr auto idx = pointer_variant_detail::find_v<self_type, result_type>;
		static_assert(idx < boost::mp11::mp_size<self_type>::value, "pointer type not from given type list");
		assert((reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK) == 0);

		m_owning = 0;
		m_type = idx;
		m_ptr = pack(static_cast<result_type>(ptr));
	}

	template <class ... PointerTypes>
	template <class T, class Type, std::enable_if_t<pointer_variant_detail::extactly_once<pointer_variant<PointerTypes...>, T>, int> /*= 0*/>
	inline pointer_variant<PointerTypes...>::pointer_variant(std::in_place_type_t<T>, Type * ptr) noexcept
	    : pointer_variant(static_cast<T>(ptr))
	{

	}

	template <class ... PointerTypes>
	template <std::size_t Index, class Type, std::enable_if_t<std::is_convertible_v<Type *, pointer_variant_alternative_t<Index, pointer_variant<PointerTypes...>>>, int> /*= 0*/>
	inline pointer_variant<PointerTypes...>::pointer_variant(std::in_place_index_t<Index>, Type * ptr) noexcept
	{
		static_assert(Index < boost::mp11::mp_size<self_type>::value, "pointer type not from given type list");
		using T = pointer_variant_alternative_t<Index, pointer_variant<PointerTypes...>>;
		assert((reinterpret_cast<std::uintptr_t>(ptr) & PTR_MASK) == 0);

		m_owning = 0;
		m_type = Index;
		m_ptr = pack(static_cast<T>(ptr));
	}

	template <class ... PointerTypes>
	template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> /*= 0*/>
	inline pointer_variant<PointerTypes...>::pointer_variant(std::unique_ptr<Type> ptr) noexcept
		: pointer_variant(ptr.release())
	{
		m_owning = 1;
	}

	template <class ... PointerTypes>
	template <class T, class Type, std::enable_if_t<pointer_variant_detail::extactly_once<pointer_variant<PointerTypes...>, T>, int> /*= 0*/>
	inline pointer_variant<PointerTypes...>::pointer_variant(std::in_place_type_t<T> hint, std::unique_ptr<Type> ptr) noexcept
	    : pointer_variant(hint, ptr.release())
	{
		m_owning = 1;
	}

	template <class ... PointerTypes>
	template <std::size_t Index, class Type, std::enable_if_t<std::is_convertible_v<Type *, pointer_variant_alternative_t<Index, pointer_variant<PointerTypes...>>>, int> /*= 0*/>
	inline pointer_variant<PointerTypes...>::pointer_variant(std::in_place_index_t<Index> hint, std::unique_ptr<Type> ptr) noexcept
	    : pointer_variant(hint, ptr.release())
	{
		m_owning = 1;
	}

	template <class ... PointerTypes>
	template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> /*= 0*/>
	inline auto pointer_variant<PointerTypes...>::operator =(Type * ptr) noexcept -> pointer_variant &	
	{
		using result_type = std::invoke_result_t<pointer_variant_detail::overload_set<PointerTypes...>, Type *>;
		constexpr auto idx = pointer_variant_detail::find_v<self_type, result_type>;
		static_assert(idx < boost::mp11::mp_size<self_type>::value, "pointer type not from given type list");

		destroy();

		m_owning = 0;
		m_type = idx;
		m_ptr = pack(static_cast<result_type>(ptr));

		return *this;
	}

	template <class ... PointerTypes>
	template <class Type, std::enable_if_t<pointer_variant_detail::invokable_v<pointer_variant<PointerTypes...>, Type *>, int> /*= 0*/>
	inline auto pointer_variant<PointerTypes...>::operator =(std::unique_ptr<Type> ptr) noexcept -> pointer_variant &	
	{
		m_owning = 1;
		return operator =(ptr.release());
	}

	// conversion constructor from variant of different pointer types
	template <class ... PointerTypes>
	template <class ... OtherPointerTypes, std::enable_if_t<pointer_variant_detail::all_invokable_v<pointer_variant<PointerTypes...>, pointer_variant<OtherPointerTypes...>>, int> /*= 0*/>
	pointer_variant<PointerTypes...>::pointer_variant(pointer_variant<OtherPointerTypes...> && other) noexcept
	{
		auto constructor = [](auto * ptr) { return self_type(ptr); };
		auto self = viewed::visit(constructor, other);

		m_val = std::exchange(self.m_val, 0);
		m_owning = other.m_owning;
		other.m_val = 0;
	}

	template <class ... PointerTypes>	
	inline pointer_variant<PointerTypes...>::pointer_variant() noexcept
	{
		m_val = 0;
	}

	template <class ... PointerTypes>
	inline void pointer_variant<PointerTypes...>::destroy() noexcept
	{
		if (m_owning)
			viewed::visit([](auto * ptr) { delete ptr; }, *this);
	}

	template <class ... PointerTypes>
	inline pointer_variant<PointerTypes...>::~pointer_variant() noexcept
	{
		destroy();
	}

	template <class ... PointerTypes>
	inline pointer_variant<PointerTypes...>::pointer_variant(pointer_variant && op) noexcept	
	{
		m_val = std::exchange(op.m_val, 0);
	}

	template <class ... PointerTypes>
	inline auto pointer_variant<PointerTypes...>::operator =(pointer_variant && op) noexcept -> pointer_variant &
	{
		if (this != &op)
		{
			std::destroy_at(this);
			new(this) pointer_variant(std::move(op));
		}

		return *this;
	}


	template <class T, class ... PointerTypes>
	constexpr inline bool holds_alternative(const pointer_variant<PointerTypes...> & v) noexcept
	{
		constexpr auto index = boost::mp11::mp_find<pointer_variant<PointerTypes...>, T>::value;
		return index == v.index();
	}

	template <class T, class ... PointerTypes>
	constexpr inline T get(const pointer_variant<PointerTypes...> & v)
	{
		constexpr auto idx = boost::mp11::mp_find<pointer_variant<PointerTypes...>, T>::value;
		static_assert(pointer_variant_detail::extactly_once<pointer_variant<PointerTypes...>, T>, "T should occur for exactly once in alternatives");
		if (v.index() != idx) throw bad_variant_access();

		return static_cast<T>(v.pointer());
	}

	template <std::size_t Index, class ... Types>
	constexpr inline auto get(const pointer_variant<Types...> & v) ->
		pointer_variant_alternative_t<Index, pointer_variant<Types...>>
	{
		using T = pointer_variant_alternative_t<Index, pointer_variant<Types...>>;
		if (v.index() != Index) throw bad_variant_access();

		return static_cast<T>(v.pointer());
	}
	/************************************************************************/
	/*            pointer_variant visit implementation                      */
	/************************************************************************/
	namespace pointer_variant_detail
	{
		template <class... Contants>
		struct join_index_constants;

		template <std::size_t ... Index>
		struct join_index_constants<std::integral_constant<std::size_t, Index>...>
		{
			using type = std::index_sequence<Index...>;
		};

		template <class ... Constatns>
		using join_index_constants_t = typename join_index_constants<Constatns...>::type;

		template <class Type>
		using mp_from_sequence_variants = boost::mp11::mp_from_sequence<std::make_index_sequence<pointer_variant_size_v<ext::remove_cvref_t<Type>>>>;


		template <class ResultType, std::size_t ... Indexes, class Visitor, class ... Variants>
		inline ResultType dispatch_function1(std::index_sequence<Indexes...>, Visitor && vis, const Variants & ... args)
		{
			static_assert(sizeof...(Indexes) == sizeof...(Variants));

			return std::forward<Visitor>(vis)(
				reinterpret_cast<pointer_variant_alternative_t<Indexes, ext::remove_cvref_t<Variants>>>(args.pointer())...
			);
		}

		template <class ResultType, class Indexes, class Visitor, class ... Variants>
		inline ResultType dispatch_function2(Visitor && vis, const Variants & ... args)
		{
			return dispatch_function1<ResultType>(Indexes {}, std::forward<Visitor>(vis), args...);
		}


		template <class ResultType, class Visitor, class Variants, class Indexes>
		struct dispatch_table; // undefined

		template <class ResultType, class Visitor, class ... Variants, class ... Indexes>
		struct dispatch_table<ResultType, Visitor, boost::mp11::mp_list<Variants...>, boost::mp11::mp_list<Indexes...>>
		{
			//using result_type = std::invoke_result_t<Visitor, boost::mp11::mp_first<ext::remove_cvref_t<Variants>>...>;
			using sig_type = ResultType(*)(Visitor &&, const Variants & ...);

			static constexpr sig_type dispatch_array[] = {&dispatch_function2<ResultType, Indexes, Visitor, Variants...>...};
		};

		template <class ... Variants>
		constexpr auto dispatch_scale_v = (pointer_variant_size_v<Variants> * ... * 1);
		
		constexpr std::size_t dispatch_index()
		{
			return 0;
		}

		template <class Variant, class ... Rest>
		constexpr std::size_t dispatch_index(const Variant & var, const Rest & ... rest)
		{
			return var.index() * dispatch_scale_v<Rest...> + dispatch_index(rest...);
		}
	} // namespace pointer_variant_detail


	template <class Visitor, class ... Variants>
	constexpr inline auto visit(Visitor && vis, Variants && ... vars) ->
		std::enable_if_t<
			std::conjunction_v<is_pointer_variant<ext::remove_cvref_t<Variants>>...>,
			std::invoke_result_t<Visitor, boost::mp11::mp_first<ext::remove_cvref_t<Variants>>...>
		>
	{
		using result_type = std::invoke_result_t<Visitor, boost::mp11::mp_first<ext::remove_cvref_t<Variants>>...>;
		return visit<result_type>(std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
	}

	template <class ResultType, class Visitor, class ... Variants>
	constexpr auto visit(Visitor && vis, Variants && ... vars) ->
		std::enable_if_t<
			std::conjunction_v<is_pointer_variant<ext::remove_cvref_t<Variants>>...>,
			ResultType
		>
	{
		using namespace pointer_variant_detail;
		using namespace boost::mp11;

		using list_of_sequence_lists = mp_product<
			join_index_constants_t,
			mp_from_sequence_variants<Variants>...
			//boost::mp11::mp_from_sequence<std::make_index_sequence<pointer_variant_size_v<ext::remove_cvref_t<Variants>>>>...
		>;

		const auto dispatch_idx = dispatch_index(vars...);
		constexpr auto & dispatch_array = dispatch_table<ResultType, Visitor, mp_list<ext::remove_cvref_t<Variants>...>, list_of_sequence_lists>::dispatch_array;
		return dispatch_array[dispatch_idx](std::forward<Visitor>(vis), std::forward<Variants>(vars)...);
	}

	/************************************************************************/
	/*          pointer_variant relation operators                          */
	/************************************************************************/
	template <class ... PointerTypes>
	constexpr inline bool operator ==(const pointer_variant<PointerTypes...> & v1, const pointer_variant<PointerTypes...> & v2)
	{
		return v1.index() == v2.index() and v1.pointer() == v2.pointer();
	}

	template <class ... PointerTypes>
	constexpr inline bool operator !=(const pointer_variant<PointerTypes...> & v1, const pointer_variant<PointerTypes...> & v2)
	{
		return not operator ==(v1, v2);
	}

	template <class ... PointerTypes>
	constexpr inline bool operator <(const pointer_variant<PointerTypes...> & v1, const pointer_variant<PointerTypes...> & v2)
	{
		return v1.index() < v1.index() or v1.index() == v2.index() and v1.pointer() < v2.pointer();
	}

	template <class ... PointerTypes>
	constexpr inline bool operator >(const pointer_variant<PointerTypes...> & v1, const pointer_variant<PointerTypes...> & v2)
	{
		return operator <(v2, v1);
	}

	template <class ... PointerTypes>
	constexpr inline bool operator <=(const pointer_variant<PointerTypes...> & v1, const pointer_variant<PointerTypes...> & v2)
	{
		return not operator >(v1, v2);
	}

	template <class ... PointerTypes>
	constexpr inline bool operator >=(const pointer_variant<PointerTypes...> & v1, const pointer_variant<PointerTypes...> & v2)
	{
		return not operator <(v1, v2);
	}
}
