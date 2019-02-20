#pragma once
#include <vector>
#include <viewed/associative_container_base.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/key.hpp>

#include <boost/mp11/list.hpp>
#include <boost/mp11/bind.hpp>
#include <boost/mp11/algorithm.hpp>

namespace viewed
{
	template <
		class Type,
		class KeyExtractor = boost::multi_index::identity<Type>,
		class Compare      = std::less<>
	>
	struct ordered_container_traits;


	template <class Type, class KeyExtractor, class Compare>
	struct ordered_container_traits
	{
		using value_type = Type;
		using key_extractor_type = KeyExtractor;
		using key_compare = Compare;

		/// container class that stores value_type,
		/// main_store_type should provide stable pointers/references,
		/// iterators allowed to be invalidated on modify operations.
		using main_store_type = boost::multi_index_container <
			value_type,
			boost::multi_index::indexed_by<
				boost::multi_index::ordered_unique<KeyExtractor, Compare>
			>
		>;

		/// container type used for storing raw pointers for views notifications
		using signal_store_type = std::vector<const value_type *>;

		/************************************************************************/
		/*                   traits functions/functors                          */
		/************************************************************************/
		/// function interface can be static function members or static functors members.
		/// if overloading isn't needed static function members  - will be ok,
		/// but if you want provide several overloads - use static functors members
		
		static main_store_type make_store(KeyExtractor key_extractor, Compare comp)
		{
			using ctor_args = typename main_store_type::ctor_args;
			return main_store_type(
				ctor_args(std::move(key_extractor), std::move(comp))
			);
		}

		/// obtains pointer from internal_value_type (from main_store_type)
		static const value_type *  value_pointer(const value_type & val)   { return &val; }
		static       value_type *  value_pointer(      value_type & val)   { return &val; }

		/// update takes current internal_value_type rvalue as first argument and some generic type as second.
		/// It updates current value with new data, usually second type is some reference of value_type
		static void update(value_type & val, const value_type &  newval) { val = newval; }
		static void update(value_type & val,       value_type && newval) { val = std::move(newval); }
	};


	/// deduces and defines types including base one for ordered_container
	template <class ... Types>
	struct ordered_container_types
	{
	private:
		static_assert(sizeof...(Types) > 0);

		using last_type = boost::mp11::mp_at_c<boost::mp11::mp_list<Types...>, sizeof...(Types) - 1>;
		using typelist  = boost::mp11::mp_if<is_signal_traits<last_type>, boost::mp11::mp_pop_front<boost::mp11::mp_list<Types...>>, boost::mp11::mp_list<Types...>>;

		static_assert(boost::mp11::mp_size<typelist>::value > 0, "Only signal_traits type is given, value_type is missing");
		using first_type = boost::mp11::mp_first<typelist>;

	public:
		/// transform given L1<T...> to ordered_container_traits<T...>
		using eval_ordered_container_traits = boost::mp11::mp_bind_front<boost::mp11::mp_apply_q, boost::mp11::mp_quote<ordered_container_traits>>;
		using traits = boost::mp11::mp_eval_if_q<is_traits_type<first_type>, first_type, eval_ordered_container_traits, typelist>;
		using signal_traits = boost::mp11::mp_eval_if<is_signal_traits<last_type>, last_type, default_signal_traits, typename traits::value_type>;

	private:
		static constexpr bool all_used = boost::mp11::mp_empty<
			boost::mp11::mp_if<is_traits_type<first_type>, boost::mp11::mp_rest<typelist>, boost::mp11::mp_list<>>
		>::value;

		static_assert(all_used, "type arguments contain unused parameters, is first one is a traits type?");
	};



	/// ordered_container is an associative container that contains a set of unique objects of given type.
	/// It is similar to unordered_set but, emits signals od data updates/assigns/clears.
	///
	/// It store data in ordered store(something similar to std::set) and does not allow duplicates, new records will replace already existing
	/// It provides forward traversal iterators, while iterators not stable, pointers and references - are stable
	/// Iterators are read-only. use upsert to add new data
	///
	/// Emits signals when elements added or erased
	/// Can be used to build views on this container, see viewed::view_base
	///
	/// @Types - is list of types in form: traits types..., signal_traits
	///   signal_traits - are always optional, but if given - should be last argument(detected via is_signal_traits)
	///   types... are either: one explicit traits type(detected via is_traits_type)
	///                    or: arguments to ordered_container_traits
	///
	/// examples:
	///   ordered_container<int>
	///   ordered_container<some_type, boost::multi_index::key<&some_type::id>>
	///   ordered_container<some_type, boost::multi_index::key<&some_type::id>, signal_traits<some_type>>
	///   ordered_container<some_type, boost::multi_index::key<&some_type::id>, std::less<>, signal_traits<some_type>>
	///
	///   ordered_container<some_traits_type>
	///   ordered_container<some_traits_type, signal_traits<some_traits_type::value_type>>
	///
	template <class ... Types>
	class ordered_container :
		public associative_container_base<
	        typename ordered_container_types<Types...>::traits,
	        typename ordered_container_types<Types...>::signal_traits
		>
	{
	public:
		using traits_type        = typename ordered_container_types<Types...>::traits;
		using signal_traits_type = typename ordered_container_types<Types...>::signal_traits;

	private:
		using base_type = associative_container_base<traits_type, signal_traits_type>;

	public:
		using key_compare = typename traits_type::key_compare;
		using key_extractor_type = typename traits_type::key_extractor_type;

		using typename base_type::const_reference;
		using typename base_type::const_iterator;
		using typename base_type::size_type;

	public:
		key_compare key_comp() const { return base_type::m_store.key_comp(); }
		key_extractor_type key_extractor() const { return base_type::m_store.key_extractor(); }
		
		template <class CompatibleKey>
		const_iterator lower_bound(const CompatibleKey & key) const { return base_type::m_store.lower_bound(key); }

		template <class CompatibleKey>
		const_iterator upper_bound(const CompatibleKey & key) const { return base_type::m_store.upper_bound(key); }

		template <class CompatibleKey>
		std::pair<const_iterator, const_iterator> equal_range(const CompatibleKey & key) const
		{ return base_type::m_store.equal_range(key); }

	public:
		ordered_container(key_extractor_type key_extractor = {}, key_compare comp = {})
			: base_type(base_type::traits_type(), std::move(key_extractor), std::move(comp)) {}

		ordered_container(traits_type traits, key_extractor_type key_extractor, key_compare comp)
			: base_type(std::move(traits), std::move(key_extractor), std::move(comp)) {}
		
		ordered_container(const ordered_container & val) = delete;
		ordered_container & operator =(const ordered_container & val) = delete;

		ordered_container(ordered_container && val) = default;
		ordered_container & operator =(ordered_container && val) = default;
	};
}
