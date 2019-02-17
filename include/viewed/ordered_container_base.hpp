#pragma once
#include <vector>
#include <viewed/associative_container_base.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/key.hpp>

namespace viewed
{
	template <class Type, class KeyExtractor, class Compare>
	struct ordered_container_traits
	{
		typedef Type value_type;

		/// container class that stores value_type,
		/// main_store_type should provide stable pointers/references,
		/// iterators allowed to be invalidated on modify operations.
		typedef boost::multi_index_container <
			value_type,
			boost::multi_index::indexed_by<
				boost::multi_index::ordered_unique<KeyExtractor, Compare>
			>
		> main_store_type;

		/// container type used for storing raw pointers for views notifications
		typedef std::vector<const value_type *> signal_store_type;

		/************************************************************************/
		/*                   traits functions/functors                          */
		/************************************************************************/
		/// function interface can be static function members or static functors members.
		/// if overloading isn't needed static function members  - will be ok,
		/// but if you want provide several overloads - use static functors members
		
		static main_store_type make_store(KeyExtractor key_extractor, Compare comp)
		{
			typedef typename main_store_type::ctor_args ctor_args;
			return main_store_type(
				ctor_args(std::move(key_extractor), std::move(comp))
			);
		}

		/// obtains pointer from internal_value_type (from main_store_type)
		static const value_type *  value_pointer(const value_type & val)   { return &val; }
		static       value_type *  value_pointer(      value_type & val)   { return &val; }

		/// update takes current internal_value_type rvalue as first argument and some generic type as second.
		/// It updates current value with new data
		/// usually second type is some reference of value_type
		struct update_type;
		static const update_type update;
	};

	template <class Type, class KeyExtractor, class Compare>
	struct ordered_container_traits<Type, KeyExtractor, Compare>::update_type
	{
		typedef void result_type;
		result_type operator()(value_type & val, const value_type & newval) const
		{
			val = newval;
		}

		result_type operator()(value_type & val, value_type && newval) const
		{
			val = std::move(newval);
		}
	};

	template <class Type, class KeyExtractor, class Compare>
	const typename ordered_container_traits<Type, KeyExtractor, Compare>::update_type
		ordered_container_traits<Type, KeyExtractor, Compare>::update = {};



	/// ordered_container_base class. You are expected to inherit it and add more functional.
	/// Generic container with stl compatible interface
	///
	/// It store data in ordered store(something similar to std::set) and does not allow duplicates, new records will replace already existing
	/// It provides forward bidirectional iterators, iterators, pointers and references - are stable
	/// Iterators are read-only. use upsert to add new data
	///
	/// Emits signals when elements added or erased
	/// Can be used to build views on this container, see viewed::view_base
	/// 
	/// @Param Type element type
	/// @Param Compare functor used to test elements for equivalence
	/// @Param Traits traits class describes various aspects of ordered container
	template <
		class Type,
		class KeyExtractor,
		class Compare = std::less<>,
		class Traits = ordered_container_traits<Type, KeyExtractor, Compare>,
		class SignalTraits = default_signal_traits<Type>
	>
	class ordered_container_base :
		public associative_container_base<Type, Traits, SignalTraits>
	{
		typedef associative_container_base<Type, Traits, SignalTraits> base_type;
		
	protected:
		typedef typename base_type::traits_type traits_type;

	public:
		typedef Compare key_compare;
		typedef KeyExtractor key_extractor_type;
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
		ordered_container_base(key_extractor_type key_extractor = {}, key_compare comp = {})
			: base_type(base_type::traits_type(), std::move(key_extractor), std::move(comp)) {}

		ordered_container_base(traits_type traits, key_extractor_type key_extractor, key_compare comp)
			: base_type(std::move(traits), std::move(key_extractor), std::move(comp)) {}
		
		ordered_container_base(const ordered_container_base & val) = delete;
		ordered_container_base & operator =(const ordered_container_base & val) = delete;

		ordered_container_base(ordered_container_base && val) = default;
		ordered_container_base & operator =(ordered_container_base && val) = default;
	};
}
