#pragma once
#include <algorithm>
#include <iterator> // for back_inserter
#include <viewed/signal_traits.hpp>
#include <viewed/algorithm.hpp>

#include <ext/try_reserve.hpp>
#include <ext/type_traits.hpp>
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

		/// constructs main_store_type with provided arguments
		static main_store_type make_store(... main_store_ctor_args);

		/// obtains pointer from internal_value_type (from main_store_type)
		static const_pointer   value_pointer(const internal_value_type & val)   { return val.get(); }
		static       pointer   value_pointer(      internal_value_type & val)   { return val.get(); }

		/// update takes current internal_value_type rvalue as first argument and some generic type as second.
		/// It updates current value with new data, usually second type is some reference of value_type
		static void update(internal_value_type & val, const value_type &  newval);
		static void update(internal_value_type & val,       value_type && newval);
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
	struct is_traits_type_lookalike : std::false_type {};

	template <class Type>
	struct is_traits_type_lookalike<Type, std::void_t<
		typename Type::value_type,
		typename Type::main_store_type,
		decltype(Type::value_pointer(std::declval<typename Type::main_store_type::value_type>()))
	>> : std::true_type {};

	/// you can specialize this trait to explicitly define if concrete type is a trait
	template <class Type>
	struct is_traits_type : is_traits_type_lookalike<Type> {};


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
		static_assert(is_traits_type<Traits>::value);
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

		size_type size() const noexcept { return m_store.size(); }
		bool empty()     const noexcept { return m_store.empty(); }

		/// signals
		template <class... Args> connection on_erase(Args && ... args)  { return m_erase_signal.connect(std::forward<Args>(args)...); }
		template <class... Args> connection on_update(Args && ... args) { return m_update_signal.connect(std::forward<Args>(args)...); }
		template <class... Args> connection on_clear(Args && ... args)  { return m_clear_signal.connect(std::forward<Args>(args)...); }

	protected:
		/// finds and updates or appends elements from [first; last) into internal store m_store
		/// those elements also placed into upserted_recs for further notifications of views
		template <class SinglePassIterator>
		void upsert_newrecs(SinglePassIterator first, SinglePassIterator last);

		template <class SinglePassIterator>
		void assign_newrecs(SinglePassIterator first, SinglePassIterator last);

		/// erases elements [first, last) from attached views
		void erase_from_views(const_iterator first, const_iterator last);

		/// notifies views about update
		void notify_views(signal_store_type & erased, signal_store_type & updated, signal_store_type & inserted);

	public:
		/// erases all elements
		void clear();
		/// erases elements [first, last) from internal store and views
		/// [first, last) must be a valid range
		const_iterator erase(const_iterator first, const_iterator last);
		/// erase element pointed by it
		const_iterator erase(const_iterator it) { return erase(it, std::next(it)); }
		/// erase element by key
		template <class CompatibleKey>
		size_type erase(const CompatibleKey & key);

		/// upserts new record from [first, last)
		/// records which are already in container will be replaced with new ones
		template <class SinglePassIterator>
		auto upsert(SinglePassIterator first, SinglePassIterator last) -> std::enable_if_t<ext::is_iterator_v<SinglePassIterator>>
		{ return upsert_newrecs(first, last); }

		template <class SinglePathRange>
		auto upsert(SinglePathRange && range) -> std::enable_if_t<ext::is_range_v<SinglePathRange>>
		{ return upsert(boost::begin(range), boost::end(range)); }

		void upsert(std::initializer_list<value_type> ilist) { upsert(std::begin(ilist), std::end(ilist)); }

		/// clear container and assigns elements from [first, last)
		template <class SinglePassIterator>
		auto assign(SinglePassIterator first, SinglePassIterator last) -> std::enable_if_t<ext::is_iterator_v<SinglePassIterator>>
		{ return assign_newrecs(first, last); }

		template <class SinglePassRange>
		auto assign(SinglePassRange && range) -> std::enable_if_t<ext::is_range_v<SinglePassRange>>
		{ return assign(boost::begin(range), boost::end(range)); }

		void assign(std::initializer_list<value_type> ilist) { assign(std::begin(ilist), std::end(ilist)); }

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
	template <class SinglePassIterator>
	void associative_container_base<Traits, SignalTraits>::upsert_newrecs
		(SinglePassIterator first, SinglePassIterator last)
	{
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
			if (inserted_into_store)
			{
				inserted.push_back(ptr);
			}
			else
			{
				traits_type::update(const_cast<value_type &>(*where), std::forward<decltype(val)>(val));
				updated.push_back(ptr);
			}
		}

		notify_views(erased, updated, inserted);
	}

	template <class Traits, class SignalTraits>
	template <class SinglePassIterator>
	void associative_container_base<Traits, SignalTraits>::assign_newrecs
		(SinglePassIterator first, SinglePassIterator last)
	{
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
			if (inserted_into_store)
			{
				inserted.push_back(ptr);
			}
			else
			{
				traits_type::update(const_cast<value_type &>(*where), std::forward<decltype(val)>(val));
				updated.push_back(ptr);

				// mark found item in erase list
				auto it = std::lower_bound(erased_first, erased_last, ptr);
				if (it != erased_last and *it == ptr) *it = viewed::mark_pointer(*it);
			}
		}

		erased_last = std::remove_if(erased_first, erased_last, viewed::marked_pointer);
		erased.erase(erased_last, erased.end());
		notify_views(erased, updated, inserted);

		auto key_extractor = m_store.key_extractor();
		for (auto * ptr : erased) m_store.erase(key_extractor(*ptr));
	}

	template <class Traits, class SignalTraits>
	void associative_container_base<Traits, SignalTraits>::erase_from_views(const_iterator first, const_iterator last)
	{
		signal_store_type todel;
		ext::try_reserve(todel, first, last);
		std::transform(first, last, std::back_inserter(todel), get_pointer);

		auto rawRange = signal_traits::make_range(todel.data(), todel.data() + todel.size());
		m_erase_signal(rawRange);
	}

	template <class Traits, class SignalTraits>
	void associative_container_base<Traits, SignalTraits>::clear()
	{
		m_clear_signal();
		m_store.clear();
	}

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
		// insert + followed update(s) -> just insert
		// update + followed update(s) -> update
		// there are will no erased duplicates

		std::sort(updated_first, updated_last);
		updated_last = std::unique(updated_first, updated_last);

		for (auto it = inserted_first; it != inserted_last; ++it)
		{
			auto ptr = *it;
			auto found_it = std::lower_bound(updated_first, updated_last, ptr);
			if (found_it != updated_last and ptr == *found_it) *it = viewed::mark_pointer(ptr);
		}

		updated_last = std::remove_if(updated_first, updated_last, viewed::marked_pointer);

		auto urr = signal_traits::make_range(updated_first, updated_last);
		auto irr = signal_traits::make_range(inserted_first, inserted_last);
		auto err = signal_traits::make_range(erased.data(), erased.data() + erased.size());
		m_update_signal(err, urr, irr);
	}


	template <class Traits, class SignalTraits>
	auto associative_container_base<Traits, SignalTraits>::erase(const_iterator first, const_iterator last) -> const_iterator
	{
		erase_from_views(first, last);
		return m_store.erase(first, last);
	}

	template <class Traits, class SignalTraits>
	template <class CompatibleKey>
	auto associative_container_base<Traits, SignalTraits>::erase(const CompatibleKey & key) -> size_type
	{
		const_iterator first, last;
		std::tie(first, last) = m_store.equal_range(key);
		auto count = std::distance(first, last);
		erase(first, last);
		return count;
	}
}
