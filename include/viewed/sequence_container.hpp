#pragma once
#include <algorithm>
#include <iterator> // for back_inserter
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <viewed/signal_traits.hpp>
#include <ext/try_reserve.hpp>
#include <ext/range/range_traits.hpp>

namespace viewed
{
	/*
	/// container_traits describes store type, etc for sequence_container.
	/// see also default_sequence_container_traits as default implementations
	struct container_traits
	{
		//////////////////////////////////////////////////////////////////////////
		///                           types
		//////////////////////////////////////////////////////////////////////////
		/// container class that stores value_type in some form,
		/// main_store_type should provide stable pointers/references,
		/// iterators allowed to be invalidated on modify operations.
		typedef implementation_defined main_store_type;

		/// container type used for storing raw pointers(const value_type *) for views notifications
		typedef implementation_defined signal_store_type;

		/// assumed internal_value_type = typename main_store_type::value_type;
		/// for example std::unique_ptr<Type>

		//////////////////////////////////////////////////////////////////////////
		///                   traits functions/functors
		//////////////////////////////////////////////////////////////////////////
		/// function interface can be static function members or static functors members.

		/// constructs main_store_type with provided arguments
		static main_store_type make_store();
		
		/// obtains reference from internal_value_type (from main_store_type)
		static const_reference value_reference(const internal_value_type & val) { return *val; }
		static       reference value_reference(      internal_value_type & val) { return *val; }

		/// obtains pointer from internal_value_type (from main_store_type)
		static const_pointer   value_pointer(const internal_value_type & val)   { return val.get(); }
		static       pointer   value_pointer(      internal_value_type & val)   { return val.get(); }

		/// Constructs element in internal_value_type form from value_type argument.
		static internal_value_type make_internal(const value_type & something) { return val; }
	};
	*/

	/// default sequence_container traits. Elements stored in vector<std::unique_ptr<Type>>
	template <class Type>
	struct default_sequence_container_traits
	{
		using internal_value_type = std::unique_ptr<Type>;
		using main_store_type     = std::vector<internal_value_type>;
		using signal_store_type   = std::vector<const Type *>;

		static main_store_type make_store()                  { return {}; }
		static auto make_internal(Type && val)               { return std::make_unique<Type>(std::move(val)); }
		static auto make_internal(const Type & val)          { return std::make_unique<Type>(val); }

		static decltype(auto) value_reference(const internal_value_type & val) { return (*val); }
		static decltype(auto) value_reference(      internal_value_type & val) { return (*val); }

		static auto value_pointer(const internal_value_type & val) { return val.get(); }
		static auto value_pointer(      internal_value_type & val) { return val.get(); }
	};

	/*
	/// signal_traits describes types used for communication between stores and views
	/// see also default_signal_traits for an example
	struct signal_traits
	{
		/// random access range of valid pointers(at least at moment of call) to value_type
		typedef implementation-defined signal_range_type;

		/// static functor/method for creating signal_range_type from some signal_store_type
		static signal_range_type make_range(const Type ** first, const Type ** last);

		/// signals should be boost::signals or compatible:
		/// * connect call, connection, scoped_connection
		/// * emission via call like: signal(args...)

		typedef implementation-defined connection;
		typedef implementation-defined scoped_connection;

		/// signal is emitted in process of updating data in container(after update/insert, before erase) with 3 ranges of pointers.
		///  * 1st to erased elements
		///  * 2nd points to elements that were updated
		///  * 3rd to newly inserted, order is unspecified
		///  signature: void(signal_range_type erased, signal_range_type updated, signal_range_type inserted)
		typedef implementation-defined update_signal_type;

		/// signal is emitted before data is erased from container,
		/// with range of pointers to elements to erase
		/// signature: void(signal_range_type erased),
		typedef implementation-defined erase_signal_type;

		/// signal is emitted before container is cleared.
		/// signature: void(),
		typedef implementation-defined clear_signal_type;
	}
	*/


	/// sequence_container is build on top of some defined by traits sequence container.
	/// Generic container with STL compatible interface.
	///
	/// It store data in store specified by traits
	/// iterators can be unstable, pointers and references - have to be stable
	/// (this is dictated by views, which stores pointers)
	/// Iterators are read-only. use assign/append/erase to add/remove data
	/// 
	/// Emits signals when elements added or erased
	/// Can be used to build views on this container, see viewed::view_base
	/// 
	/// @Param Element type
	/// @Param Traits traits class describes various aspects of container,
	///        see description above, also see hash_container_traits and ordered_container_traits for example
	/// @Param SignalTraits traits class describes various aspects of signaling,
	///        see description above, also see default_signal_traits
	template <
		class Type,
		class Traits = default_sequence_container_traits<Type>,
		class SignalTraits = default_signal_traits<Type>
	>
	class sequence_container : protected Traits
	{
		using self_type = sequence_container<Type, Traits, SignalTraits>;
	
	protected:
		using traits_type    = Traits;
		using signal_traits  = SignalTraits;
	
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
		// container interface
		using value_type      =       Type;
		using const_reference = const Type &;
		using const_pointer   = const Type *;

		using pointer   = const_pointer;
		using reference = const_reference;

		using size_type       = typename main_store_type::size_type;
		using difference_type = typename main_store_type::difference_type;

	protected:
		template <class reference_type, class iterator_base>
		class iterator_adaptor : public boost::iterator_adaptor<iterator_adaptor<reference_type, iterator_base>, iterator_base, value_type, boost::use_default, reference_type>
		{
			friend boost::iterator_core_access;
			using base_type = boost::iterator_adaptor<iterator_adaptor<reference_type, iterator_base>, iterator_base, value_type, boost::use_default, reference_type>;

		private:
			reference_type dereference() const noexcept { return self_type::value_reference(*this->base_reference()); }

		public:
			using base_type::base_type;
		};

	public:
		using const_iterator         = iterator_adaptor<const_reference, typename main_store_type::const_iterator>;
		using const_reverse_iterator = iterator_adaptor<const_reference, typename main_store_type::const_reverse_iterator>;

		using iterator         = const_iterator;
		using reverse_iterator = const_reverse_iterator;
		//using iterator         = iterator_adaptor<reference, typename main_store_type::iterator>;
		//using reverse_iterator = iterator_adaptor<reference, typename main_store_type::reverse_iterator>;

	public:
		/// forward signal types
		using connection        = typename signal_traits::connection;
		using scoped_connection = typename signal_traits::scoped_connection;

		using signal_range_type  = typename signal_traits::signal_range_type;
		using update_signal_type = typename signal_traits::update_signal_type;
		using erase_signal_type  = typename signal_traits::erase_signal_type;
		using clear_signal_type  = typename signal_traits::clear_signal_type;

	public:
		using view_pointer_type = const_pointer;
		static view_pointer_type get_view_pointer(const_reference ref)     noexcept { return &ref; }
		static const_reference   get_view_reference(view_pointer_type ptr) noexcept { return *ptr; }

	protected:
		main_store_type m_store;

		update_signal_type m_update_signal;
		erase_signal_type  m_erase_signal;
		clear_signal_type  m_clear_signal;

	public:
		      iterator begin()        noexcept { return iterator(m_store.begin()); }
		      iterator end()          noexcept { return iterator(m_store.end()); }
		const_iterator begin()  const noexcept { return const_iterator(m_store.cbegin()); }
		const_iterator end()    const noexcept { return const_iterator(m_store.cend()); }
		const_iterator cbegin() const noexcept { return const_iterator(m_store.cbegin()); }
		const_iterator cend()   const noexcept { return const_iterator(m_store.cend()); }

		      reverse_iterator rbegin()        noexcept { return reverse_iterator(m_store.rbegin()); }
		      reverse_iterator rend()          noexcept { return reverse_iterator(m_store.rend()); }
		const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator(m_store.crbegin()); }
		const_reverse_iterator rend()    const noexcept { return const_reverse_iterator(m_store.crend()); }
		const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(m_store.crbegin()); }
		const_reverse_iterator crend()   const noexcept { return const_reverse_iterator(m_store.crend()); }

		size_type size() const noexcept { return m_store.size(); }
		bool empty()     const noexcept { return m_store.empty(); }

		      reference front()       noexcept { return self_type::value_reference(m_store.front()); }
		const_reference front() const noexcept { return self_type::value_reference(m_store.front()); }

		      reference back()       noexcept { return self_type::value_reference(m_store.back()); }
		const_reference back() const noexcept { return self_type::value_reference(m_store.back()); }

		      reference at(size_type idx)       { return self_type::value_reference(m_store.at(idx)); }
		const_reference at(size_type idx) const { return self_type::value_reference(m_store.at(idx)); }

		      reference operator [](size_type idx)       noexcept { return self_type::value_reference(m_store.operator[](idx)); }
		const_reference operator [](size_type idx) const noexcept { return self_type::value_reference(m_store.operator[](idx)); }

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
		void assign(SinglePassIterator first, SinglePassIterator last);

		template <class SinglePassIterator>
		iterator insert(const_iterator where, SinglePassIterator first, SinglePassIterator last);

		/// appends new record from [first, last)
		template <class SinglePassIterator>
		void append(SinglePassIterator first, SinglePassIterator last) { insert(end(), first, last); }

		template <class Modifier>
		void modify(const_iterator first, const_iterator last, Modifier modifier);


		template <class Arg> void append(Arg && arg)    { append(&arg, std::next(&arg)); }
		template <class Arg> void push_back(Arg && arg) { return append(std::forward<Arg>(arg)); }

		template <class Arg> iterator insert(const_iterator where, Arg && arg) { return insert(where, &arg, std::next(&arg)); }

		/// erases elements [first, last)
		/// [first, last) must be a valid range
		const_iterator erase(const_iterator first, const_iterator last);
		/// erase element pointed by it
		const_iterator erase(const_iterator it) { return erase(it, std::next(it)); }

		/// erases all elements
		void clear();

	public:
		sequence_container(traits_type traits = {})
			: traits_type(std::move(traits)), m_store(traits_type::make_store())
		{};

		sequence_container(const sequence_container &) = delete;
		sequence_container & operator =(const sequence_container &) = delete;

		sequence_container(sequence_container && op) = default;
		sequence_container & operator =(sequence_container && op) = default;
	};




	template <class Type, class Traits, class SignalTraits>
	void sequence_container<Type, Traits, SignalTraits>::notify_views
		(signal_store_type & erased, signal_store_type & updated, signal_store_type & inserted)
	{
		auto urr = signal_traits::make_range(updated.data(), updated.data() + updated.size());
		auto irr = signal_traits::make_range(inserted.data(), inserted.data() + inserted.size());
		auto err = signal_traits::make_range(erased.data(), erased.data() + erased.size());
		m_update_signal(err, urr, irr);
	}

	template <class Type, class Traits, class SignalTraits>
	template <class SinglePassIterator>
	void sequence_container<Type, Traits, SignalTraits>::assign
		(SinglePassIterator first, SinglePassIterator last)
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, size_type>);

		signal_store_type erased, updated, inserted;
		ext::try_reserve(inserted, first, last);

		erased.resize(m_store.size());

		auto erased_first = erased.begin();
		auto erased_last = erased.end();
		std::transform(m_store.begin(), m_store.end(), erased_first, get_pointer);

		for (; first != last; ++first)
		{
			m_store.push_back(self_type::make_internal(std::move(*first)));
			inserted.push_back(get_pointer(m_store.back()));
		}

		notify_views(erased, updated, inserted);
		m_store.erase(m_store.begin(), m_store.begin() + erased.size());
	}

	template <class Type, class Traits, class SignalTraits>
	template <class SinglePassIterator>
	auto sequence_container<Type, Traits, SignalTraits>::insert
		(const_iterator where, SinglePassIterator first, SinglePassIterator last) -> iterator
	{
		static_assert(std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, value_type>);

		signal_store_type erased, updated, inserted;

		auto make_internal = [](auto && val) { return self_type::make_internal(std::forward<decltype(val)>(val)); };

		auto oldsize = m_store.size();
		auto result = m_store.insert(where.base(), boost::make_transform_iterator(first, make_internal), boost::make_transform_iterator(last, make_internal));
		auto inserted_count = m_store.size() - oldsize;

		inserted.resize(inserted_count);
		std::transform(result, result + inserted_count, inserted.begin(), get_pointer);

		notify_views(erased, updated, inserted);
		return iterator(result);
	}

	template <class Type, class Traits, class SignalTraits>
	template <class Modifier>
	void sequence_container<Type, Traits, SignalTraits>::modify
		(const_iterator first, const_iterator last, Modifier modifier)
	{
		signal_store_type erased, updated, inserted;

		iterator mfirst = m_store.begin();
		iterator mlast = m_store.begin();

		mfirst = first + (first - mfirst);
		mlast  = first + (last  - mfirst);

		for (auto it = mfirst; it != mlast; ++it)
			modifier(get_reference(*it));

		updated.resize(last - first);
		std::transform(first, last, updated.begin(), get_pointer);

		notify_views(erased, updated, inserted);
	}

	template <class Type, class Traits, class SignalTraits>
	auto sequence_container<Type, Traits, SignalTraits>::erase(const_iterator first, const_iterator last) -> const_iterator
	{
		signal_store_type todel;
		std::transform(first, last, std::back_inserter(todel), get_pointer);

		auto rawRange = signal_traits::make_range(todel.data(), todel.data() + todel.size());
		m_erase_signal(rawRange);

		return m_store.erase(first, last);
	}

	template <class Type, class Traits, class SignalTraits>
	void sequence_container<Type, Traits, SignalTraits>::clear()
	{
		m_clear_signal();
		m_store.clear();
	}
}
