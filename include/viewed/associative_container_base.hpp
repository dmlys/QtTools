#pragma once
#include <algorithm>
#include <iterator> // for back_inserter
#include <viewed/signal_traits.hpp>
#include <viewed/algorithm.hpp>

#include <ext/try_reserve.hpp>
#include <ext/type_traits.hpp>
#include <ext/utility.hpp>
#include <ext/range/range_traits.hpp>

namespace viewed
{
	/*
	/// container_traits describes store type, update on exist action, etc
	struct container_traits
	{
		//////////////////////////////////////////////////////////////////////////
		///                           types
		//////////////////////////////////////////////////////////////////////////
		/// Element type
		using value_type = something;

		/// container class that stores value_type,
		/// main_store_type should provide stable pointers/references,
		/// iterators allowed to be invalidated on modify operations.
		using main_store_type = implementation_defined;
		
		/// container type used for storing raw pointers for views notifications
		using signal_store_type = implementation_defined;

		/// assumed internal_value_type = typename main_store_type::value_type;

		//////////////////////////////////////////////////////////////////////////
		///                   traits functions/functors
		//////////////////////////////////////////////////////////////////////////
		/// function interface can be static function members or static functor members.

		/// extracts key_extractor from store_type
		static key_extractor_type key_extractor(const main_store_type & store) { return store.key_extractor(); }
		/// constructs main_store_type with provided arguments
		static main_store_type make_store(... main_store_ctor_args);

		/// modifies element pointed by it in store via modifier, if renaming is not possibly - restores element via rollback
		template <class Modifier, class Rollback>
		static void rename(main_store_type & store, typename main_store_type::const_iterator it, Modifier & modifier, Rollback && rollback)
		{ store.modify(it, modifier, std::forward<Rollback>(rollback));	}

		/// obtains pointer from internal_value_type (from main_store_type)
		static const_pointer   value_pointer(const internal_value_type & val)   { return val.get(); }
		static       pointer   value_pointer(      internal_value_type & val)   { return val.get(); }

		/// obtains reference from internal_value_type (from main_store_type)
		static decltype(auto) value_reference(const value_type & val) { return *val; }
		static decltype(auto) value_reference(      value_type & val) { return *val; }
	};
	*/

	/*
	/// signal_traits describes types used for communication between stores and views
	struct signal_traits
	{
		/// random access range of valid pointers(at least at moment of call) to value_type
		using signal_range_type = implementation-defined;
		
		/// static functor/method for creating signal_range_type from some signal_store_type
		static signal_range_type make_range(const Type ** first, const Type ** last);

		/// signals should be boost::signals or compatible:
		/// * connect call, connection, scoped_connection
		/// * emission via call like: signal(args...)
		
		using connection        = implementation-defined;
		using scoped_connection = implementation-defined;

		/// signal is emitted in process of updating data in container(after update/insert, before erase) with 3 ranges of pointers.
		///  * 1st to erased elements
		///  * 2nd points to elements that were updated
		///  * 3rd to newly inserted, order is unspecified
		///  signature: void(signal_range_type erased, signal_range_type updated, signal_range_type inserted)
		using update_signal_type = implementation-defined;

		/// signal is emitted before data is erased from container,
		/// with range of pointers to elements to erase
		/// signature: void(signal_range_type erased),
		using erase_signal_type = implementation-defined;

		/// signal is emitted before container is cleared.
		/// signature: void(),
		using clear_signal_type = implementation-defined;
	}
	*/

	template <class Type, class = void>
	struct is_associative_container_traits_type_lookalike : std::false_type {};

	template <class Type>
	struct is_associative_container_traits_type_lookalike<Type, std::void_t<
		typename Type::value_type,
		typename Type::main_store_type,
		decltype(Type::value_pointer(std::declval<typename Type::main_store_type::value_type>()))
	>> : std::true_type {};

	/// you can specialize this trait to explicitly define if concrete type is a trait
	template <class Type>
	struct is_associative_container_traits_type : is_associative_container_traits_type_lookalike<Type> {};


	/// Base associative container class built on top of some defined by traits associative container.
	/// Usually you will use more specialized class like hash_container or ordered container.
	/// Generic container with STL compatible interface.
	///
	/// It store data in store specified by traits
	/// iterators can be unstable, pointers and references - have to be stable
	/// (this is dictated by views, which stores pointers)
	/// Iterators are read-only. use upsert to add new data
	///
	/// Emits signals when elements added or erased
	/// Can be used to build views on this container, see viewed::view_base
	/// 
	/// @Param Traits traits class describes value_type and various aspects of container,
	///        see description above, also see hash_container_traits and ordered_container_traits for example
	/// @Param SignalTraits traits class describes various aspects of signaling,
	///        see description above, also see default_signal_traits
	template <
		class Traits,
		class SignalTraits = default_signal_traits<typename Traits::value_type>
	>
	class associative_container_base : protected Traits
	{
		static_assert(is_associative_container_traits_type<Traits>::value);
		using self_type = associative_container_base<Traits, SignalTraits>;
	
	protected:
		using traits_type   = Traits;
		using signal_traits = SignalTraits;
	
		using main_store_type   = typename traits_type::main_store_type;
		using signal_store_type = typename traits_type::signal_store_type;

		struct get_reference_type
		{
			template <class type> decltype(auto) operator()(type && val) const noexcept { return self_type::value_reference(std::forward<type>(val)); }
		};

		struct get_pointer_type
		{
			template <class type> decltype(auto) operator()(type & val) const noexcept { return self_type::value_pointer(std::forward<type>(val)); }
		};

		static constexpr get_reference_type get_reference {};
		static constexpr get_pointer_type   get_pointer {};

	public:
		//container interface
		using key_type        = typename main_store_type::key_type;
		using value_type      = typename traits_type::value_type;
		using const_reference = const value_type &;
		using const_pointer   = const value_type *;

		using pointer   = const_pointer;
		using reference = const_reference;

		using size_type = typename main_store_type::size_type;
		using difference_type = typename main_store_type::difference_type;

		using const_iterator = typename main_store_type::const_iterator;
		using iterator       =                           const_iterator;

	public:
		/// forward signal types
		using connection        = typename signal_traits::connection;
		using scoped_connection = typename signal_traits::scoped_connection;

		using signal_range_type  = typename signal_traits::signal_range_type;
		using update_signal_type = typename signal_traits::update_signal_type;
		using erase_signal_type  = typename signal_traits::erase_signal_type;
		using clear_signal_type  = typename signal_traits::clear_signal_type;

	public:
		// view related pointer helpers
		using view_pointer_type = const_pointer;
		static view_pointer_type get_view_pointer(const_reference ref)     noexcept { return &ref; }
		static const_reference   get_view_reference(view_pointer_type ptr) noexcept { return *ptr; }

	protected:
		main_store_type m_store;

		update_signal_type m_update_signal;
		erase_signal_type  m_erase_signal;
		clear_signal_type  m_clear_signal;

	public:
		const_iterator begin()  const noexcept { return m_store.cbegin(); }
		const_iterator end()    const noexcept { return m_store.cend(); }
		const_iterator cbegin() const noexcept { return m_store.cbegin(); }
		const_iterator cend()   const noexcept { return m_store.cend(); }

		template <class CompatibleKey>
		const_iterator find(const CompatibleKey & key) const { return m_store.find(key); }
		template <class CompatibleKey>
		const_iterator count(const CompatibleKey & key) const { return m_store.count(key); }

		template <class CompatibleKey>
		std::pair<const_iterator, const_iterator> equal_range(const CompatibleKey & key) const
		{ return m_store.equal_range(key); }

		size_type size() const noexcept { return m_store.size(); }
		bool empty()     const noexcept { return m_store.empty(); }

		/// signals
		template <class... Args> connection on_erase(Args && ... args)  { return m_erase_signal.connect(std::forward<Args>(args)...); }
		template <class... Args> connection on_update(Args && ... args) { return m_update_signal.connect(std::forward<Args>(args)...); }
		template <class... Args> connection on_clear(Args && ... args)  { return m_clear_signal.connect(std::forward<Args>(args)...); }

	protected:
		/// notifies views about update
		void notify_views(signal_store_type & erased, signal_store_type & updated, signal_store_type & inserted);

	public:
		/// clear container and assigns elements from [first, last)
		template <class SinglePassIterator>
		void assign(SinglePassIterator first, SinglePassIterator last) { return assign(first, last, viewed::default_assigner); }
		/// clear container and assigns elements from [first, last)
		/// updater - functor used to assign records: updater(existing_record, *curit);
		template <class SinglePassIterator, class Updater>
		void assign(SinglePassIterator first, SinglePassIterator last, Updater updater);

		/// upserts new record from [first, last)
		/// records which are already in container will be replaced with new ones
		template <class SinglePassIterator>
		void upsert(SinglePassIterator first, SinglePassIterator last) { return upsert(first, last, viewed::default_assigner); }
		/// upserts new record from [first, last)
		/// records which are already in container will be replaced with new ones
		/// updater - functor used to update records: updater(oldrec, *curit);
		template <class SinglePassIterator, class Updater>
		void upsert(SinglePassIterator first, SinglePassIterator last, Updater updater);

		/// modifies elements from [first; last) via given modifier
		template <class Modifier>
		void modify(const_iterator first, const_iterator last, Modifier modifier);
		/// modifies element pointed by it via given modifier
		template <class Modifier>
		void modify(const_iterator it, Modifier modifier) { return modify(it, std::next(it), std::move(modifier)); }
		/// modifies elements specified by keys from [first; last) via given modifier
		template <class SinglePassIterator, class Modifier>
		void modify(SinglePassIterator first, SinglePassIterator last, Modifier modifier);
		/// modifies element specified by key via given modifier
		template <class CompatibleKey, class Modifier>
		void modify(const CompatibleKey & key, Modifier modifier) { return modify(&key, std::next(&key), std::move(modifier)); }

		/// renames/modifies elements from [first; last) via given modifier
		/// if new name clashes with already present - element is removed(same as boost::multi_index)
		template <class Modifier>
		void rename(const_iterator first, const_iterator last, Modifier modifier);
		/// renames/modifies element pointed by it via given modifier
		/// if new name clashes with already present - element is removed(same as boost::multi_index)
		template <class Modifier>
		void rename(const_iterator it, Modifier modifier) { return rename(it, std::next(it), std::move(modifier)); }
		/// renames/modifies elements specified by keys from [first; last) via given modifier
		/// if new name clashes with already present - element is removed(same as boost::multi_index)
		template <class SinglePassIterator, class Modifier>
		void rename(SinglePassIterator first, SinglePassIterator last, Modifier modifier);
		/// renames/modifies element specified by key via given modifier
		/// if new name clashes with already present - element is removed(same as boost::multi_index)
		template <class CompatibleKey, class Modifier>
		void rename(const CompatibleKey & key, Modifier modifier) { return rename(&key, std::next(&key), std::move(modifier)); }

		/// erases elements [first, last) from internal store and views
		/// [first, last) must be a valid range
		const_iterator erase(const_iterator first, const_iterator last);
		/// erase element pointed by it
		const_iterator erase(const_iterator it) { return erase(it, std::next(it)); }
		/// erase element by key
		template <class CompatibleKey>
		size_type erase(const CompatibleKey & key) { return erase(&key, std::next(&key)); }
		/// erases elements specified by keys from [first; last)
		template <class SinglePassIterator>
		size_type erase(SinglePassIterator first, SinglePassIterator last);

		/// erases all elements
		void clear();

	protected:
		associative_container_base(traits_type traits = {})
		    : traits_type(std::move(traits))
		{};

		template <class ... StoreArgs>
		associative_container_base(traits_type traits, StoreArgs && ... storeArgs)
			: traits_type(std::move(traits)),
			  m_store(traits_type::make_store(std::forward<StoreArgs>(storeArgs)...))
		{};

		associative_container_base(const associative_container_base &) = delete;
		associative_container_base & operator =(const associative_container_base &) = delete;

		associative_container_base(associative_container_base && op) = default;
		associative_container_base & operator =(associative_container_base && op) = default;
	};





	template <class Traits, class SignalTraits>
	void associative_container_base<Traits, SignalTraits>::notify_views
		(signal_store_type & erased, signal_store_type & updated, signal_store_type & inserted)
	{
		auto inserted_first = inserted.data();
		auto inserted_last  = inserted_first + inserted.size();
		auto updated_first  = updated.data();
		auto updated_last   = updated_first + updated.size();

		// both assign_newrecs and upsert_newrecs can produce duplicates:
		// * several updates
		// * insert + update
		// we must sanitize this, views expect that element is either inserted, updated or erased
		//   insert + followed update(s) -> just insert
		//   update + followed update(s) -> update
		//   there are will no be erased duplicates

		std::sort(updated_first, updated_last);
		updated_last = std::unique(updated_first, updated_last);

		for (auto it = inserted_first; it != inserted_last; ++it)
		{
			auto ptr = *it;
			auto found_it = std::lower_bound(updated_first, updated_last, ptr);
			if (found_it != updated_last and ptr == *found_it) *found_it = viewed::mark_pointer(ptr);
		}

		updated_last = std::remove_if(updated_first, updated_last, viewed::marked_pointer);

		auto urr = signal_traits::make_range(updated_first, updated_last);
		auto irr = signal_traits::make_range(inserted_first, inserted_last);
		auto err = signal_traits::make_range(erased.data(), erased.data() + erased.size());
		m_update_signal(err, urr, irr);
	}

	template <class Traits, class SignalTraits>
	template <class SinglePassIterator, class Updater>
	void associative_container_base<Traits, SignalTraits>::assign
		(SinglePassIterator first, SinglePassIterator last, Updater updater)
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, value_type>);

		signal_store_type erased, updated, inserted;
		ext::try_reserve(updated, first, last);
		ext::try_reserve(inserted, first, last);

		erased.resize(m_store.size());

		auto erased_first = erased.begin();
		auto erased_last = erased.end();
		std::transform(m_store.begin(), m_store.end(), erased_first, get_pointer);
		std::sort(erased_first, erased_last);

		for (; first != last; ++first)
		{
			auto && val = *first;

			typename main_store_type::const_iterator where;
			bool inserted_into_store;
			std::tie(where, inserted_into_store) = m_store.insert(std::forward<decltype(val)>(val));

			auto * ptr = get_pointer(*where);
			auto & ref = get_reference(*where);
			if (inserted_into_store)
			{
				inserted.push_back(ptr);
			}
			else
			{
				updater(ext::unconst(ref), std::forward<decltype(val)>(val));
				updated.push_back(ptr);

				// mark found item in erase list
				auto it = std::lower_bound(erased_first, erased_last, ptr);
				if (it != erased_last and *it == ptr) *it = viewed::mark_pointer(*it);
			}
		}

		erased_last = std::remove_if(erased_first, erased_last, viewed::marked_pointer);
		erased.erase(erased_last, erased.end());
		notify_views(erased, updated, inserted);

		auto key_extractor = traits_type::key_extractor(m_store);
		for (auto * ptr : erased) m_store.erase(key_extractor(*ptr));
	}


	template <class Traits, class SignalTraits>
	template <class SinglePassIterator, class Updater>
	void associative_container_base<Traits, SignalTraits>::upsert
		(SinglePassIterator first, SinglePassIterator last, Updater updater)
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, value_type>);

		signal_store_type erased, updated, inserted;
		ext::try_reserve(updated, first, last);
		ext::try_reserve(inserted, first, last);

		for (; first != last; ++first)
		{
			auto && val = *first;

			typename main_store_type::const_iterator where;
			bool inserted_into_store;
			std::tie(where, inserted_into_store) = m_store.insert(std::forward<decltype(val)>(val));

			auto * ptr = get_pointer(*where);
			auto & ref = get_reference(*where);
			if (inserted_into_store)
			{
				inserted.push_back(ptr);
			}
			else
			{
				updater(ext::unconst(ref), std::forward<decltype(val)>(val));
				updated.push_back(ptr);
			}
		}

		notify_views(erased, updated, inserted);
	}

	template <class Traits, class SignalTraits>
	template <class Modifier>
	void associative_container_base<Traits, SignalTraits>::modify
		(const_iterator first, const_iterator last, Modifier modifier)
	{
		signal_store_type erased, updated, inserted;
		ext::try_reserve(updated, first, last);

		for (; first != last; ++first)
		{
			auto * ptr = get_pointer(*first);
			auto & ref = get_reference(*first);

			modifier(ext::unconst(ref));
			updated.push_back(ptr);
		}

		notify_views(erased, updated, inserted);
	}

	template <class Traits, class SignalTraits>
	template <class SinglePassIterator, class Modifier>
	void associative_container_base<Traits, SignalTraits>::modify
		(SinglePassIterator first, SinglePassIterator last, Modifier modifier)
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, key_type>);

		signal_store_type erased, updated, inserted;
		ext::try_reserve(updated, first, last);

		for (; first != last; ++first)
		{
			auto && key = *first;

			auto found_it = m_store.find(key);
			if (found_it == m_store.end())
				continue;

			auto * ptr = get_pointer(*found_it);
			auto & ref = get_reference(*found_it);

			modifier(ext::unconst(ref));
			updated.push_back(ptr);
		}

		notify_views(erased, updated, inserted);
	}

	template <class Traits, class SignalTraits>
	template <class Modifier>
	void associative_container_base<Traits, SignalTraits>::rename
		(const_iterator first, const_iterator last, Modifier modifier)
	{
		signal_store_type erased, updated, inserted;
		ext::try_reserve(updated, first, last);

		auto key_extractor = traits_type::key_extractor(m_store);

		for (; first != last; ++first)
		{
			auto * ptr = get_pointer(*first);
			updated.push_back(ptr);

			auto rollback = [key_extractor, key = key_extractor(*first)](auto & item) { key_extractor(item) = key; };
			traits_type::rename(m_store, first, modifier, std::move(rollback));
		}

		notify_views(erased, updated, inserted);
	}

	template <class Traits, class SignalTraits>
	template <class SinglePassIterator, class Modifier>
	void associative_container_base<Traits, SignalTraits>::rename
		(SinglePassIterator first, SinglePassIterator last, Modifier modifier)
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, key_type>);

		signal_store_type erased, updated, inserted;
		ext::try_reserve(updated, first, last);

		auto key_extractor = traits_type::key_extractor(m_store);

		for (; first != last; ++first)
		{
			auto && key = *first;

			auto found_it = m_store.find(key);
			if (found_it == m_store.end())
				continue;

			auto * ptr = get_pointer(*found_it);
			updated.push_back(ptr);

			auto rollback = [key_extractor, key](auto & item) { key_extractor(item) = key; };
			traits_type::rename(m_store, found_it, modifier, std::move(rollback));
		}

		notify_views(erased, updated, inserted);
	}


	template <class Traits, class SignalTraits>
	auto associative_container_base<Traits, SignalTraits>::erase(const_iterator first, const_iterator last) -> const_iterator
	{
		signal_store_type todel;
		std::transform(first, last, std::back_inserter(todel), get_pointer);

		auto rawRange = signal_traits::make_range(todel.data(), todel.data() + todel.size());
		m_erase_signal(rawRange);

		return m_store.erase(first, last);
	}

	template <class Traits, class SignalTraits>
	template <class SinglePassIterator>
	auto associative_container_base<Traits, SignalTraits>::erase(SinglePassIterator first, SinglePassIterator last) -> size_type
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, key_type>);

		using iterator_pair = std::pair<const_iterator, const_iterator>;
		std::vector<iterator_pair> erased_pairs;
		signal_store_type todel;

		ext::try_reserve(erased_pairs, first, last);

		for (; first != last; ++first)
		{
			auto && key = *first;

			const_iterator ifirst, ilast;
			std::tie(ifirst, ilast) = m_store.equal_range(key);
			std::transform(ifirst, ilast, std::back_inserter(todel), get_pointer);
			erased_pairs.push_back(std::make_pair(ifirst, ilast));
		}

		auto rawRange = signal_traits::make_range(todel.data(), todel.data() + todel.size());
		m_erase_signal(rawRange);

		for (auto & [first, last] : erased_pairs)
			m_store.erase(first, last);

		return todel.size();
	}

	template <class Traits, class SignalTraits>
	void associative_container_base<Traits, SignalTraits>::clear()
	{
		m_clear_signal();
		m_store.clear();
	}
}
