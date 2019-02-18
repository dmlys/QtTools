#pragma once
#include <vector>
#include <ext/noinit.hpp>
#include <ext/range/range_traits.hpp>

#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace viewed
{
	/// This class provides base for building views based on viewed containers.
	/// 
	/// it provides base for other views:
	///  * vector of pointers
	///  * STL compatible interface, indirected, not pointers
	///  * it connects signals from container to handlers(which are virtual and can be overridden)
	///    basic implementation does almost nothing, just synchronizes view with data owning container
	/// 
	/// Main container expected to live as long as view does. View holds only non owning pointers to data
	/// 
	/// container must meet following conditions:
	/// * STL compatible interface(types, methods)
	/// * view_pointer_type - pointer typedef, typically const value_type *
	///   get_view_pointer(const_reference ref) -> view_pointer_type - static member function/functor
	///   get_view_reference(view_pointer_type ptr) -> const_reference - static member function/functor
	///   
	/// * at least forward iterator category, but pointers/references must be stable, iterators can be unstable
	/// * have member type:
	///     signal_range_type - random access range of valid pointers(at least at moment of call) to value_type
	///     scoped_connection - owning signal connection handle, breaks connection in destructor
	///     
	/// * on_update member function which connects given functor with signal and returns connection.
	///             slot signature is: void (signal_range_type erased, signal_range_type updated, signal_range_type inserted).
	///             signal is emitted in process of updating data in container(after update/insert, before erase) with 3 ranges of pointers.
	///             1st points to removed elements, 2nd points to elements that were updated, 3nd to newly inserted.
	///             
	/// * on_erase member function which connects given functor with signal and returns connection.
	///             slot signature is: void (signal_range_type erased).
	///             signal is emitted before data is erased from container, with range of pointers to elements to erase.
	///             
	/// * on_clear member function which connects given functor with signal and returns connection.
	///             slot signature is: void ().
	///             signal is emitted before container is cleared.
	/// 
	/// 
	/// this class is intended to be inherited and extended to provide more functionality
	/// sorting, filtering, and may be more
	/// 
	/// derived classes should reimplement:
	///  * update_data
	///  * reinit_view
	///  * clear_view
	///  * erase_records
	/// (see method declaration for more description)
	///
	/// @Param Container class to which this view will connect and listen updates, described above
	template <class Container>
	class view_base
	{
		typedef view_base self_type;

	public:
		typedef Container container_type;
		/// container interface typedefs
		typedef typename container_type::value_type      value_type;
		typedef typename container_type::const_pointer   const_pointer;
		typedef typename container_type::const_reference const_reference;
		typedef const_reference                          reference;
		typedef const_pointer                            pointer;

	protected:
		// store <-> view exchange
		typedef typename container_type::view_pointer_type view_pointer_type;
		static_assert(std::is_pointer_v<view_pointer_type>);

		struct get_view_pointer_type
		{
			view_pointer_type operator()(const value_type & val) const noexcept { return container_type::get_view_pointer(val); }
		};

		struct get_view_reference_type
		{
			const_reference operator()(view_pointer_type ptr) const noexcept { return container_type::get_view_reference(ptr); }
		};

		static constexpr get_view_pointer_type   get_view_pointer {};
		static constexpr get_view_reference_type get_view_reference {};

	protected:
		typedef typename container_type::signal_range_type   signal_range_type;
		typedef typename container_type::scoped_connection   scoped_connection;
		typedef std::vector<view_pointer_type>               store_type;

	public:
		typedef typename store_type::size_type           size_type;
		typedef typename store_type::difference_type     difference_type;

	protected:
		template <class iterator_base>
		class iterator_adaptor : public boost::iterator_adaptor<iterator_adaptor<iterator_base>, iterator_base, value_type, boost::use_default, const_reference>
		{
			friend boost::iterator_core_access;
			using base_type = boost::iterator_adaptor<iterator_adaptor<iterator_base>, iterator_base, value_type, boost::use_default, const_reference>;

		private:
			decltype(auto) dereference() const noexcept { return self_type::get_view_reference(*this->base_reference()); }

		public:
			using base_type::base_type;
		};

	public:
		using const_iterator         = iterator_adaptor<typename store_type::const_iterator>;
		using const_reverse_iterator = iterator_adaptor<typename store_type::const_reverse_iterator>;

		using iterator         = const_iterator;
		using reverse_iterator = const_reverse_iterator;

	protected:
		container_type * m_owner = nullptr; // pointer to owning container, can be used by derived classes
		store_type m_store; // view store, can be used by derived classes

		/// raii connections
		scoped_connection m_clear_con;
		scoped_connection m_update_con;
		scoped_connection m_erase_con;

	public:
		/// container interface
		const_iterator begin() const noexcept  { return const_iterator(m_store.begin()); }
		const_iterator end() const noexcept    { return const_iterator(m_store.end()); }
		const_iterator cbegin() const noexcept { return const_iterator(m_store.cbegin()); }
		const_iterator cend() const noexcept   { return const_iterator(m_store.cend()); }

		const_reverse_iterator rbegin() const noexcept  { return const_reverse_iterator(m_store.rbegin()); }
		const_reverse_iterator rend() const noexcept    { return const_reverse_iterator(m_store.rend()); }
		const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(m_store.crbegin()); }
		const_reverse_iterator crend() const noexcept   { return const_reverse_iterator(m_store.crend()); }

		const_reference at(size_type idx) const { return get_view_reference(m_store.at(idx)); }
		const_reference operator [](size_type idx) const noexcept { return get_view_reference(m_store.operator[](idx)); }
		const_reference front() const { return get_view_reference(m_store.front()); }
		const_reference back()  const { return get_view_reference(m_store.back()); }

		size_type size() const noexcept { return m_store.size(); }
		bool empty()     const noexcept { return m_store.empty(); }

	public:
		/// returns pointer to owning container
		container_type * get_owner() const noexcept { return m_owner; }

		/// connects container signals to appropriate handlers
		virtual void connect_signals();
		/// reinitializes view
		/// default implementation just copies from owner
		virtual void reinit_view();
		/// Normally should not be called outside of view class.
		/// Calls connects_signals and calls reinit_view.
		/// 
		/// Derived views probably will automatically call it from constructor
		/// or directly connect_signals/reinit_view
		virtual void init();

	protected:
		/// sorts erased ranges by pointer value, so we can use binary search on them
		virtual void prepare_erase(const signal_range_type & erased);
		virtual void prepare_update(
			const signal_range_type & erased,
			const signal_range_type & updated,
			const signal_range_type & inserted);

	protected:
		/// container event handlers, those are called on container signals,
		/// you could reimplement them to provide proper handling of your view
		
		/// called when new data is updated in owning container
		/// view have to synchronize itself.
		/// @Param sorted_erased range of pointers to erased records, sorted by pointer value
		/// @Param updated range of pointers to updated records
		/// @Param inserted range of pointers to inserted
		/// 
		/// default implementation removes erased, appends inserted records, and does nothing with sorted_updated
		virtual void update_data(
			const signal_range_type & sorted_erased,
			const signal_range_type & updated,
			const signal_range_type & inserted);

		/// called when some records are erased from container
		/// view have to synchronize itself.
		/// @Param sorted_erased range of pointers to erased records, sorted by pointer value
		/// 
		/// default implementation, erases those records from main store
		virtual void erase_records(const signal_range_type & sorted_erased);

		/// called when container is cleared, clears m_store.
		virtual void clear_view();
		
	protected:
		/// removes from m_store records from recs
		/// complexity N log2 M,
		/// where N = m_store.size(), M = recs.size()
		void sorted_erase_records(const signal_range_type & sorted_erased);

	protected:
		view_base(ext::noinit_type noinit, container_type * owner) : m_owner(owner) {}

	public:
		view_base(container_type * owner) : view_base(ext::noinit, owner) { this->init(); }
		virtual ~view_base() = default;

		view_base(const view_base &) = delete;
		view_base& operator =(const view_base &) = delete;

		// we have signals move constructor and operator cannot be used
		view_base(view_base &&) = delete;
		view_base& operator =(view_base &&) = delete;
		
	}; //class view_base

	template <class Container>
	void view_base<Container>::connect_signals()
	{
		auto onclear = [this]()
		{
			clear_view();
		};

		auto onupdate = [this](const signal_range_type & e, const signal_range_type & u, const signal_range_type & i)
		{
			prepare_update(e, u, i);
			update_data(e, u, i);
		};

		auto onerase = [this](const signal_range_type & r)
		{
			prepare_erase(r);
			erase_records(r);
		};


		m_clear_con = m_owner->on_clear(onclear);
		m_update_con = m_owner->on_update(onupdate);
		m_erase_con = m_owner->on_erase(onerase);
	}

	template <class Container>
	void view_base<Container>::reinit_view()
	{
		auto rng = *m_owner | boost::adaptors::transformed(get_view_pointer);
		ext::assign(m_store, boost::begin(rng), boost::end(rng));
	}

	template <class Container>
	void view_base<Container>::init()
	{
		connect_signals();
		reinit_view();
	}

	template <class Container>
	void view_base<Container>::prepare_erase(const signal_range_type & erased)
	{
		std::sort(erased.begin(), erased.end());
	}

	template <class Container>
	void view_base<Container>::prepare_update(
		const signal_range_type & erased,
		const signal_range_type & updated,
		const signal_range_type & inserted)
	{
		std::sort(erased.begin(), erased.end());
	}


	template <class Container>
	void view_base<Container>::update_data(
		const signal_range_type & sorted_erased,
		const signal_range_type & updated,
		const signal_range_type & inserted)
	{
		auto first = m_store.begin();
		auto last = m_store.end();

		if (not sorted_erased.empty())
		{
			auto pred = [&sorted_erased](view_pointer_type rec) { return boost::binary_search(sorted_erased, rec); };
			last = boost::remove_if(m_store, pred);
		}

		auto old_sz = last - first;
		m_store.resize(old_sz + inserted.size());
		boost::copy(inserted, m_store.begin() + old_sz);
	}

	template <class Container>
	void view_base<Container>::erase_records(const signal_range_type & sorted_erased)
	{
		sorted_erase_records(sorted_erased);
	}

	template <class Container>
	void view_base<Container>::clear_view()
	{
		m_store.clear();
	}

	template <class Container>
	void view_base<Container>::sorted_erase_records(const signal_range_type & sorted_erased)
	{
		if (sorted_erased.empty()) return;

		auto pred = [&sorted_erased](view_pointer_type rec) { return boost::binary_search(sorted_erased, rec); };
		boost::remove_erase_if(m_store, pred);
	}
}
