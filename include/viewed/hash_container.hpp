#pragma once
#include <vector>
#include <type_traits>
#include <viewed/associative_container_base.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
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
		class Hash         = std::hash<std::decay_t<std::invoke_result_t<KeyExtractor, Type>>>,
		class Equal        = std::equal_to<std::decay_t<std::invoke_result_t<KeyExtractor, Type>>>
	>
	struct hash_container_traits;


	template <class Type, class KeyExtractor, class Hash, class Equal>
	struct hash_container_traits
	{
		using value_type = Type;
		using key_extractor_type = KeyExtractor;
		using hasher = Hash;
		using key_equal = Equal;

		/// container class that stores value_type,
		/// main_store_type should provide stable pointers/references,
		/// iterators allowed to be invalidated on modify operations.
		using main_store_type = boost::multi_index_container <
			value_type,
			boost::multi_index::indexed_by<
				boost::multi_index::hashed_unique<KeyExtractor, Hash, Equal>
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
				
		static main_store_type make_store(KeyExtractor key_extractor, Hash hash, Equal eq)
		{
			using ctor_args = typename main_store_type::ctor_args ;
			/// The first element of this tuple indicates the minimum number of buckets
			/// set up by the index on construction time.
			/// If the default value 0 is used, an implementation defined number is used instead.
			return main_store_type(
				ctor_args(0, std::move(key_extractor), std::move(hash), std::move(eq))
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


	/// deduces and defines types including base one for hash_container
	template <class ... Types>
	struct hash_container_types
	{
		static_assert(sizeof...(Types) > 0);

		using last_type = boost::mp11::mp_at_c<boost::mp11::mp_list<Types...>, sizeof...(Types) - 1>;
		using typelist  = boost::mp11::mp_if<is_signal_traits<last_type>, boost::mp11::mp_pop_front<boost::mp11::mp_list<Types...>>, boost::mp11::mp_list<Types...>>;

		static_assert(boost::mp11::mp_size<typelist>::value > 0, "Only signal_traits type is given, value_type is missing");
		using first_type = boost::mp11::mp_first<typelist>;

		/// transform given L1<T...> to hash_container_traits<T...>
		using eval_hash_container_traits = boost::mp11::mp_bind_front<boost::mp11::mp_apply_q, boost::mp11::mp_quote<hash_container_traits>>;
		using traits = boost::mp11::mp_eval_if_q<is_associative_container_traits_type<first_type>, first_type, eval_hash_container_traits, typelist>;
		using signal_traits = boost::mp11::mp_eval_if<is_signal_traits<last_type>, last_type, default_signal_traits, typename traits::value_type>;

		static constexpr bool all_used = boost::mp11::mp_empty<
			boost::mp11::mp_if<is_associative_container_traits_type<first_type>, boost::mp11::mp_rest<typelist>, boost::mp11::mp_list<>>
		>::value;

		static_assert(all_used, "type arguments contain unused parameters, is first one is a traits type?");
	};



	/// hash_container is an associative container that contains a set of unique objects of given type.
	/// It is similar to unordered_set but, emits signals od data updates/assigns/clears.
	///
	/// It store data in hashed store and does not allow duplicates, new records will replace already existing
	/// It provides forward traversal iterators, while iterators not stable, pointers and references - are stable
	/// Iterators are read-only. use upsert to add new data
	///
	/// Emits signals when elements added or erased
	/// Can be used to build views on this container, see viewed::view_base
	/// 
	/// @Types - is list of types in form: traits types..., signal_traits
	///   signal_traits - are always optional, but if given - should be last argument(detected via is_signal_traits)
	///   types... are either: one explicit traits type(detected via is_associative_container_traits_type)
	///                    or: arguments to hash_container_traits
	///
	/// examples:
	///   hash_container<int>
	///   hash_container<some_type, boost::multi_index::key<&some_type::id>>
	///   hash_container<some_type, boost::multi_index::key<&some_type::id>, signal_traits<some_type>>
	///   hash_container<some_type, boost::multi_index::key<&some_type::id>, std::hash<some_type>, std::equal_to<>, signal_traits<some_type>>
	///
	///   hash_container<some_traits_type>
	///   hash_container<some_traits_type, signal_traits<some_traits_type::value_type>>
	///
	template <class ... Types>
	class hash_container :
		public associative_container_base<
	        typename hash_container_types<Types...>::traits,
	        typename hash_container_types<Types...>::signal_traits
	    >
	{
	public:
		using traits_type        = typename hash_container_types<Types...>::traits;
		using signal_traits_type = typename hash_container_types<Types...>::signal_traits;

	private:
		using base_type = associative_container_base<traits_type, signal_traits_type>;

	public:
		using key_equal = typename traits_type::key_equal;
		using hasher    = typename traits_type::hasher;
		using key_extractor_type = typename traits_type::key_extractor_type;

		using typename base_type::const_iterator;
		using typename base_type::const_reference;

	public:
		key_equal key_eq() const { return base_type::m_store.key_eq(); }
		hasher    hash_function() const { return base_type::m_store.hash_function(); }
		key_extractor_type key_extractor() const { return base_type::m_store.key_extractor(); }
		
		template <class CompatibleKey>
		std::pair<const_iterator, const_iterator> equal_range(const CompatibleKey & key) const
		{ return base_type::m_store.equal_range(key); }
		
	public:
		hash_container(key_extractor_type key_extractor = {}, hasher hash = {}, key_equal eq = {})
		    : base_type(typename base_type::traits_type {}, std::move(key_extractor), std::move(hash), std::move(eq))
		{ }

		hash_container(traits_type traits, key_extractor_type key_extractor, hasher hash, key_equal eq)
			: base_type(std::move(traits), std::move(key_extractor), std::move(hash), std::move(eq))
		{ }
		
		hash_container(const hash_container & val) = delete;
		hash_container& operator =(const hash_container & val) = delete;

		hash_container(hash_container && val) = default;
		hash_container & operator =(hash_container && val) = default;
	};
}
