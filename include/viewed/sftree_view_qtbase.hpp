#pragma once
#include <ext/noinit.hpp>
#include <viewed/sftree_facade_qtbase.hpp>
#include <viewed/sftree_is_base_of.hpp>

namespace viewed
{
	template <class ... Types>
	struct sftree_view_base_type
	{
		static_assert(sizeof...(Types) >= 2, "insufficient template parameters to sftree_view_qtbase");
		using list = boost::mp11::mp_list<Types...>;
		using container_type = boost::mp11::mp_at_c<list, 0>;

		using facade_types = boost::mp11::mp_drop_c<list, 1>;
		using maybe_traits_type = boost::mp11::mp_at_c<facade_types, 0>;

		using type = std::conditional_t
		<
		    sftree_detail::is_base_of<sftree_facade_qtbase, maybe_traits_type>::value,
		    maybe_traits_type,
		    boost::mp11::mp_rename<facade_types, sftree_facade_qtbase>
		>;
	};

	/// Types are:
	/// either Container + Traits + ModelBase, same as in sftree_facade_qtbase
	/// or     Container + sftree_facade_qtbase<Trait, ModelBase> derived class
	template <class ... Types>
	class sftree_view_qtbase : public sftree_view_base_type<Types...>::type
	{
		using self_type = sftree_view_qtbase;
		using base_type = typename sftree_view_base_type<Types...>::type;

	public:
		using container_type = typename sftree_view_base_type<Types...>::container_type;

	protected:
		// store <-> view exchange
		using view_pointer_type = typename container_type::view_pointer_type;
		using view_reference_type = typename container_type::const_reference;
		static_assert(std::is_pointer_v<view_pointer_type>);

		using signal_range_type = typename container_type::signal_range_type;
		using scoped_connection = typename container_type::scoped_connection;

		struct get_view_pointer_type
		{
			view_pointer_type operator()(view_reference_type val) const noexcept { return container_type::get_view_pointer(val); }
		};

		struct get_view_reference_type
		{
			view_reference_type operator()(view_pointer_type ptr) const noexcept { return container_type::get_view_reference(ptr); }
		};

	protected:
		static constexpr get_view_pointer_type   get_view_pointer {};
		static constexpr get_view_reference_type get_view_reference {};

	protected:
		std::shared_ptr<container_type> m_owner = nullptr; // pointer to owning container

		/// raii connections
		scoped_connection m_clear_con;
		scoped_connection m_update_con;
		scoped_connection m_erase_con;

	protected:
		/// container event handlers, those are called on container signals,
		/// you could reimplement them to provide proper handling of your view

		/// called when new data is updated in owning container
		/// view have to synchronize itself.
		/// @Param erased range of pointers to erased records
		/// @Param updated range of pointers to updated records
		/// @Param inserted range of pointers to inserted
		/// 
		/// default implementation removes erased, appends inserted records, and does nothing with updated
		virtual void update_data(
			const signal_range_type & erased,
			const signal_range_type & updated,
			const signal_range_type & inserted);

		/// called when some records are erased from container
		/// view have to synchronize itself.
		/// @Param erased range of pointers to updated records
		/// 
		/// default implementation, erases those records from main store
		virtual void erase_records(const signal_range_type & erased);

		/// called when container is cleared, clears m_store.
		virtual void clear_view();

	public:
		/// returns pointer to owning container
		const auto & get_owner() const noexcept { return m_owner; }

		/// connects container signals to appropriate handlers
		virtual void connect_signals();
		/// reinitializes view
		/// default implementation just copies from owner
		virtual void reinit_view();
		/// reinitializes view and notifies anyone via qt beginResetModel/endResetModel signals
		/// default implementation emits beginResetModel, calls reinit_view, emits endResetModel
		virtual void reinit_view_and_notify();
		/// normally should not be called outside of view class.
		/// Provided, when view class used directly without inheritance, to complete initialization.
		/// Calls connects signals and calls reinit_view.
		/// 
		/// Derived views probably will automatically call it constructor
		/// or directly connect_signals/reinit_view
		virtual void init();

	protected:
		sftree_view_qtbase(ext::noinit_type noinit, std::shared_ptr<container_type> owner, QObject * parent)
			: base_type(parent), m_owner(std::move(owner)) {}

	public:
		sftree_view_qtbase(std::shared_ptr<container_type> owner, QObject * parent = nullptr)
			: sftree_view_qtbase(ext::noinit, std::move(owner), parent) { this->init(); }

		virtual ~sftree_view_qtbase() = default;
	};

	/************************************************************************/
	/*                     method implementation                            */
	/************************************************************************/
	template <class ... Types>
	void sftree_view_qtbase<Types...>::reinit_view()
	{
		using leaf_pointer_vector = std::vector<view_pointer_type>;
		using reset_context = typename base_type::template reset_context_template<typename leaf_pointer_vector::iterator>;

		leaf_pointer_vector elements;
		elements.assign(
		    boost::make_transform_iterator(m_owner->begin(), get_view_pointer),
		    boost::make_transform_iterator(m_owner->end(), get_view_pointer)
		);

		this->m_root.nvisible = 0;
		this->m_root.children.clear();

		typename base_type::ivalue_ptr_vector valptr_array;
		reset_context ctx;

		auto first = elements.begin();
		auto last  = elements.end();
		this->group_by_paths(first, last);

		ctx.vptr_array = &valptr_array;
		ctx.first = first;
		ctx.last  = last;
		this->reset_page(this->m_root, ctx);
	}

	template <class ... Types>
	void sftree_view_qtbase<Types...>::reinit_view_and_notify()
	{
		this->beginResetModel();
		this->reinit_view();
		this->endResetModel();
	}

	template <class ... Types>
	void sftree_view_qtbase<Types...>::update_data(
	    const signal_range_type & erased, const signal_range_type & updated, const signal_range_type & inserted)
	{
		auto erased_first   = erased.begin();
		auto erased_last    = erased.end();
		auto updated_first  = updated.begin();
		auto updated_last   = updated.end();
		auto inserted_first = inserted.begin();
		auto inserted_last  = inserted.end();

		this->group_by_paths(erased_first, erased_last);
		this->group_by_paths(updated_first, updated_last);
		this->group_by_paths(inserted_first, inserted_last);

		return this->update_data_and_notify(
		    erased_first, erased_last,
		    updated_first, updated_last,
		    inserted_first, inserted_last);
	}

	template <class ... Types>
	void sftree_view_qtbase<Types...>::erase_records(const signal_range_type & erased)
	{
		signal_range_type updated, inserted;
		return update_data(erased, updated, inserted);
	}

	template <class ... Types>
	void sftree_view_qtbase<Types...>::clear_view()
	{
		this->beginResetModel();
		this->m_root.children.clear();
		this->m_root.nvisible = 0;
		this->endResetModel();
	}

	template <class ... Types>
	void sftree_view_qtbase<Types...>::init()
	{
		connect_signals();
		reinit_view();
	}

	template <class ... Types>
	void sftree_view_qtbase<Types...>::connect_signals()
	{
		m_clear_con  = m_owner->on_clear([this] { clear_view(); });
		m_update_con = m_owner->on_update([this](const signal_range_type & e, const signal_range_type & u, const signal_range_type & i) { update_data(e, u, i); });
		m_erase_con  = m_owner->on_erase([this](const signal_range_type & r) { erase_records(r); });
	}
}
