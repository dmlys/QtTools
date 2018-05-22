#pragma once
#include <vector>
#include <ext/range/range_traits.hpp>
#include <viewed/qt_model.hpp>
#include <viewed/view_base.hpp>
#include <viewed/algorithm.hpp>

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace viewed
{
	/// This class provides base for building views based on viewed containers.
	/// It also has knowledge of Qt models and it's signals(beginInsertRows, layoutUpdated, etc),
	/// see also view_base.
	/// 
	/// It knows about qt and emits/calls appropriate methods where needed,
	/// like layoutUpdated, beginInsertRows, etc
	///
	/// Class itself does not inherit QAbstractItemModel, but acquires it via virtual method get_model -
	/// default implementation uses dynamic_cast.
	template <class Container>
	class view_qtbase : public view_base<Container>
	{
		typedef view_qtbase            self_type;
		typedef view_base<Container>   base_type;

	public:
		using typename base_type::container_type;
		using typename base_type::view_pointer_type;

	protected:
		using typename base_type::store_type;
		using typename base_type::signal_range_type;

		using base_type::m_store;
		using base_type::m_owner;

		typedef std::vector<int> int_vector;
		typedef viewed::AbstractItemModel model_type;

	public:
		/// reinitializes view
		/// default implementation just copies from owner
		/// calls qt beginResetModel/endResetModel
		virtual void reinit_view();

	protected:
		/// acquires pointer to qt model, normally you would inherit both QAbstractItemModel and this class.
		/// default implementation uses dynamic_cast
		virtual model_type * get_model();
		/// emits qt signal model->dataChanged about changed rows. Changred rows are defined by [first; last)
		/// default implantation just calls get_model->dataChanged(index(row, 0), inex(row, model->columnCount)
		virtual void emit_changed(int_vector::const_iterator first, int_vector::const_iterator last);
		/// changes persistent indexes via get_model->changePersistentIndex.
		/// [first; last) - range where range[oldIdx - offset] => newIdx.
		/// if newIdx < 0 - index should be removed(changed on invalid, qt supports it)
		virtual void change_indexes(int_vector::const_iterator first, int_vector::const_iterator last, int offset);

	protected:
		/// container event handlers, those are called on container signals,
		/// you could reimplement them to provide proper handling of your view
		
		/// called when new data is updated in owning container
		/// view have to synchronize itself.
		/// @Param sorted_newrecs range of pointers to updated records, sorted by pointer value
		/// 
		/// default implementation removes erased, appends inserted records, and does nothing with sorted_updated
		/// emits qt beginInsertRows/endInsertRows
		virtual void update_data(
			const signal_range_type & sorted_erased,
			const signal_range_type & sorted_updated,
			const signal_range_type & inserted) override;

		/// called when some records are erased from container
		/// view have to synchronize itself.
		/// @Param sorted_newrecs range of pointers to updated records, sorted by pointer value
		/// 
		/// default implementation, erases those records from main store
		/// calls qt layoutAboutToBeChanged/layoutChanged
		virtual void erase_records(const signal_range_type & sorted_erased) override;

		/// called when container is cleared, clears m_store.
		/// calls qt beginResetModel/endResetModel
		virtual void clear_view() override;
		
	public:
		view_qtbase(container_type * owner) : base_type(owner) { }
		virtual ~view_qtbase() = default;

		view_qtbase(const view_qtbase &) = delete;
		view_qtbase& operator =(const view_qtbase &) = delete;

		// we have signals move constructor and operator cannot be used
		view_qtbase(view_qtbase &&) = delete;
		view_qtbase& operator =(view_qtbase &&) = delete;
		
	}; //class view_qtbase

	template <class Container>
	auto view_qtbase<Container>::get_model() -> model_type *
	{
		auto * model = dynamic_cast<QAbstractItemModel *>(this);
		assert(model);
		return static_cast<model_type *>(model);
	}

	template <class Container>
	void view_qtbase<Container>::emit_changed(int_vector::const_iterator first, int_vector::const_iterator last)
	{
		auto * model = get_model();
		return viewed::emit_changed(model, first, last);
	}

	template <class Container>
	void view_qtbase<Container>::change_indexes(int_vector::const_iterator first, int_vector::const_iterator last, int offset)
	{
		auto * model = get_model();
		return viewed::change_indexes(model, first, last, offset);
	}

	template <class Container>
	void view_qtbase<Container>::reinit_view()
	{
		auto * model = get_model();
		model->beginResetModel();
		base_type::reinit_view();
		model->endResetModel();
	}

	template <class Container>
	void view_qtbase<Container>::update_data(
		const signal_range_type & sorted_erased,
		const signal_range_type & sorted_updated,
		const signal_range_type & inserted)
	{
		int_vector affected_indexes(std::max(sorted_erased.size(), sorted_updated.size()));
		int_vector::iterator affected_first, affected_last;
		
		// find/emit changes
		if (not sorted_updated.empty())
		{
			auto first = m_store.begin();
			auto last  = m_store.end();
			affected_first = affected_last = affected_indexes.begin();

			auto is_updated = [&sorted_updated](view_pointer_type ptr) { return boost::binary_search(sorted_updated, ptr); };
			for (auto it = std::find_if(first, last, is_updated); it != last; it = std::find_if(++it, last, is_updated))
				*affected_last++ = static_cast<int>(it - first);
			
			emit_changed(affected_first, affected_last);
		}


		if (sorted_erased.empty())
		{
			if (not inserted.empty())
			{
				// just inserts, simple beginInsertRows/endInsertRows case
				int first = static_cast<int>(m_store.size());
				int last  = static_cast<int>(m_store.size() + inserted.size() - 1);

				auto * model = get_model();
				model->beginInsertRows(model_type::invalid_index, first, last);
				boost::push_back(m_store, inserted);
				model->endInsertRows();
			}
		}
		else
		{
			// some erased, some inserted -> more complex layoutAboutToBeChanged/layoutAboutToBeChanged case
			auto first = m_store.begin();
			auto last  = m_store.end();
			affected_first = affected_last = affected_indexes.begin();

			auto is_erased = [&sorted_erased](view_pointer_type ptr) { return boost::binary_search(sorted_erased, ptr); };
			for (auto it = std::find_if(first, last, is_erased); it != last; it = std::find_if(++it, last, is_erased))
				*affected_last++ = static_cast<int>(it - first);

			auto * model = get_model();
			Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model->NoLayoutChangeHint);

			auto index_map = viewed::build_relloc_map(affected_first, affected_last, m_store.size());
			change_indexes(index_map.begin(), index_map.end(), 0);

			last = viewed::remove_indexes(first, last, affected_first, affected_last);
			
			auto old_sz = last - first;
			m_store.resize(old_sz + inserted.size());
			boost::copy(inserted, m_store.begin() + old_sz);

			Q_EMIT model->layoutChanged(model_type::empty_model_list, model->NoLayoutChangeHint);
		}
	}

	template <class Container>
	void view_qtbase<Container>::erase_records(const signal_range_type & sorted_erased)
	{
		if (sorted_erased.empty()) return;

		auto test = [&sorted_erased](view_pointer_type ptr)
		{
			return boost::binary_search(sorted_erased, ptr);
		};

		int_vector affected_indexes(sorted_erased.size());
		auto erased_first = affected_indexes.begin();
		auto erased_last = erased_first;
		auto first = m_store.begin();
		auto last = m_store.end();

		for (auto it = std::find_if(first, last, test); it != last; it = std::find_if(++it, last, test))
			*erased_last++ = static_cast<int>(it - first);

		auto * model = get_model();
		Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model->NoLayoutChangeHint);

		auto index_map = viewed::build_relloc_map(erased_first, erased_last, m_store.size());
		change_indexes(index_map.begin(), index_map.end(), 0);

		last = viewed::remove_indexes(first, last, erased_first, erased_last);
		m_store.resize(last - first);
		
		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->NoLayoutChangeHint);
	}

	template <class Container>
	void view_qtbase<Container>::clear_view()
	{
		auto * model = get_model();
		model->beginResetModel();
		base_type::clear_view();
		model->endResetModel();
	}
}
