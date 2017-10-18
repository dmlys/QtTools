#pragma once
#include <viewed/sfview_qtbase.hpp>
#include <ext/algorithm/slide.hpp>
#include <boost/container/flat_set.hpp>

namespace viewed
{
	/// see also view_base description for more information
	/// 
	/// selectable_sfview_qtbase is sorted and filtered based on provided SortPred, FilterPred.
	/// Those can be: simple predicate or boost::variant of different predicates.
	///
	/// In derived class you can provide methods like sort_by/filter_by,
	/// which will configure those predicates
	/// 
	/// it also provides an ability to select some elements(mark them as selected)
	/// and then partition on those elements
	/// 
	/// @Param Container - class to which this view will connect and listen updates, see view_base for more description
	/// @Param SortPred - sort predicate or boost::variant of predicates,
	///                   example: std::less<Type>, boost::varaint<std::less<Type>, std::greater<Type>
	/// @Param FilterPred - filter predicate or boost::variant of predicates,
	///                     NOTE: boost::variant is'n supported yet for filter predicate
	template <
		class Container,
		class SortPred,
		class FilterPred
	>
	class selectable_sfview_qtbase : public sfview_qtbase<Container, SortPred, FilterPred>
	{
		typedef sfview_qtbase<Container, SortPred, FilterPred> base_type;
		typedef selectable_sfview_qtbase<Container, SortPred, FilterPred> self_type;
	
	public:
		using typename base_type::container_type;
		using typename base_type::sort_pred_type;
		using typename base_type::filter_pred_type;

		using typename base_type::value_type;
		using typename base_type::const_pointer;
		using typename base_type::const_iterator;
		using typename base_type::iterator;

	protected:
		using typename base_type::store_type;
		using typename base_type::int_vector;
		using typename base_type::signal_range_type;
		using typename base_type::search_hint_type;
		using typename base_type::store_iterator;
		using typename base_type::model_type;

		using base_type::m_owner;
		using base_type::m_store;
		using base_type::get_model;
		using base_type::make_pointer;

	protected:
		typedef boost::container::flat_set<const value_type *> selection_set_type;
		selection_set_type m_selection_set;

		bool m_partition_by_selection = false;
		bool m_partition_by_selection_asc = true;

	public:
		bool is_partiotioned_by_selection() const { return m_partition_by_selection; }
		bool is_partiotioned_by_selection_asc() const { return m_partition_by_selection_asc; }

		iterator select(iterator it)                    { return set_selected(it, true); }
		iterator deselect(iterator it)                  { return set_selected(it, false); }
		iterator toggle_selected(iterator it)           { return set_selected(it, !is_selected(it)); }

		/// returns a const range of selected elements,
		/// order of elements is undefined
		const selection_set_type & seleted_elements() const { return m_selection_set; }

		/// sets element by it to selected state.
		/// adjusts order if we are partitioned by selection.
		/// returns iterator after adjustion.
		virtual iterator set_selected(iterator it, bool selected);
		virtual bool is_selected(const_iterator it) const   { return m_selection_set.count(*it.base()); }

		/// sets element by it to selected state.
		/// adjusts order if we are partitioned by selection.
		/// returns iterator after adjustion.
		/// emits appropriate qt signals
		virtual iterator select_and_notify(iterator it, bool selected);

		/// emits qt beginResetModel/endResetModel
		virtual void clear_selection();

	protected:
		/// rotates m_store so, that:
		///   if {ext_it} was part of partition it moved just after partition
		///   if {ext_it} was not part of partition it moved at the end of partition
		/// this method must be called only when m_partition_by_selection == true
		/// returns new position of element pointer by ext_it
		virtual auto adjust_partition(iterator ext_it) -> iterator;
		virtual auto adjust_partition(iterator first, iterator last) -> std::pair<iterator, iterator>;

		virtual void erase_records(const signal_range_type & sorted_erased) override;
		virtual void clear_view() override;
		
		virtual void partition(store_iterator first, store_iterator last);
		virtual void partition(store_iterator first, store_iterator last,
		                       int_vector::iterator ifirst, int_vector::iterator ilast);

		virtual void partition_and_notify(store_iterator first, store_iterator last);

		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		virtual void merge_newdata(
			store_iterator first, store_iterator middle, store_iterator last,
			bool resort_old = true) override;

		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		/// 
		/// range [ifirst, imiddle, ilast) must be permuted the same way as range [first, middle, last)
		virtual void merge_newdata(
			store_iterator first, store_iterator middle, store_iterator last,
			int_vector::iterator ifirst, int_vector::iterator imiddle, int_vector::iterator ilast,
			bool resort_old = true) override;

		/// sorts [first; last) with m_sort_pred, stable sorts
		virtual void sort(store_iterator first, store_iterator last) override;
		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		/// range [ifirst; ilast) must be permuted the same way as range [first; last)
		virtual void sort(store_iterator first, store_iterator last,
		                  int_vector::iterator ifirst, int_vector::iterator ilast) override;

		/// get pair of iterators that hints where to search element
		virtual search_hint_type search_hint(const_pointer ptr) const override;

	protected:
		selectable_sfview_qtbase(container_type * owner,
		                       sort_pred_type sortPred = {},
		                       filter_pred_type filterPred = {})
			: base_type(owner, std::move(sortPred), std::move(filterPred))
		{}

		virtual ~selectable_sfview_qtbase() = default;
		selectable_sfview_qtbase(const selectable_sfview_qtbase &) = delete;
		selectable_sfview_qtbase & operator =(const selectable_sfview_qtbase &) = delete;
	};

	template <class Container, class SortPred, class FilterPred>
	auto selectable_sfview_qtbase<Container, SortPred, FilterPred>::set_selected(iterator it, bool selected) -> iterator
	{
		auto ptr = *it.base();

		if (!m_partition_by_selection)
		{
			if (selected)
				m_selection_set.insert(ptr);
			else
				m_selection_set.erase(ptr);
			return it;
		}
		else
		{
			auto sel_it = m_selection_set.lower_bound(ptr);
			bool found = sel_it != m_selection_set.end() && *sel_it == ptr;

			// selected | found    action
			//    0     |   0      do nothing
			//    0     |   1      insert and adjust
			//    1     |   0      erase  and adjust
			//    1     |   1      do nothing
			if (selected ^ found)
			{
				it = adjust_partition(it);
				if (selected)
					m_selection_set.insert(sel_it, ptr);
				else
					m_selection_set.erase(sel_it);
			}

			return it;
		}
	}

	template <class Container, class SortPred, class FilterPred>
	auto selectable_sfview_qtbase<Container, SortPred, FilterPred>::select_and_notify(iterator it, bool selected) -> iterator
	{
		auto newIt = set_selected(it, selected);
		
		int row = it.base() - m_store.begin();
		int dest = newIt.base() - m_store.begin();

		auto * model = get_model();
		if (row == dest)
		{
			auto ncol = model->columnCount(model->invalid_index);
			auto topLeft = model->index(row, 0, model->invalid_index);
			auto bottomRight = model->index(row, ncol - 1, model->invalid_index);
			Q_EMIT model->dataChanged(topLeft, bottomRight, model->all_roles);
		}
		else
		{
			// index correction for qt models
			if (dest > row) ++dest;
			bool allowed = model->beginMoveRows(model->invalid_index, row, row, model->invalid_index, dest);
			if (allowed) model->endMoveRows();
		}

		return newIt;
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::clear_selection()
	{
		// we clearing all selection, if we are partitioned by selection
		// cleared elements must be rotated out of partitioned part,
		// but because we are clearing - that part will become empty,
		// so we can leave it as is
		
		auto * model = get_model();
		model->beginResetModel();
		m_selection_set.clear();
		model->endResetModel();
	}

	template <class Container, class SortPred, class FilterPred>
	auto selectable_sfview_qtbase<Container, SortPred, FilterPred>::adjust_partition(iterator ext_it) -> iterator
	{
		assert(m_partition_by_selection);
		auto it = m_store.begin() + (ext_it.base() - m_store.begin()); // make iterator from const_iterator
		decltype(it) pp;

		if (m_partition_by_selection_asc) {
			auto pred = [this](pointer ptr) { return m_selection_set.count(ptr) != 0; };
			pp = std::partition_point(m_store.begin(), m_store.end(), pred);
		}
		else {
			auto pred = [this](pointer ptr) { return m_selection_set.count(ptr) == 0; };
			pp = std::partition_point(m_store.begin(), m_store.end(), pred);
		}

		return ext::slide(it, it + 1, pp).first;
	}

	template <class Container, class SortPred, class FilterPred>
	auto selectable_sfview_qtbase<Container, SortPred, FilterPred>::adjust_partition(iterator first, iterator last) -> std::pair<iterator, iterator>
	{
		assert(m_partition_by_selection);
		// make iterator from const_iterator
		auto ifirst = m_store.begin() + (first.base() - m_store.begin());
		auto ilast = m_store.begin() + (last.base() - m_store.begin());
		decltype(ifirst) pp;

		if (m_partition_by_selection_asc)
		{
			auto pred = [this](pointer ptr) { return m_selection_set.count(ptr) != 0; };
			pp = std::partition_point(m_store.begin(), m_store.end(), pred);
		}
		else
		{
			auto pred = [this](pointer ptr) { return m_selection_set.count(ptr) == 0; };
			pp = std::partition_point(m_store.begin(), m_store.end(), pred);
		}

		return ext::slide(ifirst, ilast, pp);
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::erase_records(const signal_range_type & sorted_erased)
	{
		for (auto ptr : sorted_erased)
			m_selection_set.erase(ptr);

		base_type::erase_records(sorted_erased);
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::clear_view()
	{
		m_selection_set.clear();
		base_type::clear_view();
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::
		partition(store_iterator first, store_iterator last)
	{
		typedef const value_type * pointer;
		if (m_partition_by_selection_asc)
		{
			auto pred = [this](pointer ptr) { return m_selection_set.count(ptr) != 0; };
			std::stable_partition(first, last, pred);
		}
		else
		{
			auto pred = [this](pointer ptr) { return m_selection_set.count(ptr) == 0; };
			std::stable_partition(first, last, pred);
		}
	}

	template <class Container, class SortPred, class FilterPred>
	void viewed::selectable_sfview_qtbase<Container, SortPred, FilterPred>::
		partition(store_iterator first, store_iterator last,
		          int_vector::iterator ifirst, int_vector::iterator ilast)
	{
		auto zfirst = ext::make_zip_iterator(first, ifirst);
		auto zlast = ext::make_zip_iterator(last, ilast);

		if (m_partition_by_selection_asc)
		{
			auto pred = [this](const value_type * ptr) { return m_selection_set.count(ptr) != 0; };
			std::stable_partition(zfirst, zlast, make_get_functor<0>(pred));
		}
		else
		{
			auto pred = [this](const value_type * ptr) { return m_selection_set.count(ptr) == 0; };
			std::stable_partition(zfirst, zlast, make_get_functor<0>(pred));
		}
	}

	template <class Container, class SortPred, class FilterPred>
	void viewed::selectable_sfview_qtbase<Container, SortPred, FilterPred>::
		partition_and_notify(store_iterator first, store_iterator last)
	{
		auto * model = get_model();
		Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model->VerticalSortHint);

		int offset = first - m_store.begin();
		int_vector indexes(last - first);
		auto ifirst = indexes.begin();
		auto ilast = indexes.end();
		std::iota(ifirst, ilast, offset);

		partition(first, last, ifirst, ilast);

		viewed::inverse_index_array(ifirst, ilast, offset);
		change_indexes(ifirst, ilast, offset);

		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->VerticalSortHint);
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::
		merge_newdata(store_iterator first, store_iterator middle, store_iterator last, bool resort_old)
	{
		if (m_partition_by_selection)
			return partition(first, last);
		else
			return base_type::merge_newdata(first, middle, last, resort_old);
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::
		merge_newdata(store_iterator first, store_iterator middle, store_iterator last,
		              int_vector::iterator ifirst, int_vector::iterator imiddle, int_vector::iterator ilast,
		              bool resort_old)
	{
		if (m_partition_by_selection)
			return partition(first, last, ifirst, ilast);
		else
			return base_type::merge_newdata(first, middle, last, ifirst, imiddle, ilast, resort_old);
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::sort(store_iterator first, store_iterator last)
	{
		if (m_partition_by_selection)
			return partition(first, last);
		else
			return base_type::sort(first, last);
	}

	template <class Container, class SortPred, class FilterPred>
	void selectable_sfview_qtbase<Container, SortPred, FilterPred>::
		sort(store_iterator first, store_iterator last,
		     int_vector::iterator ifirst, int_vector::iterator ilast)
	{
		if (m_partition_by_selection)
			return partition(first, last, ifirst, ilast);
		else
			return base_type::sort(first, last, ifirst, ilast);
	}

	template <class Container, class SortPred, class FilterPred>
	auto selectable_sfview_qtbase<Container, SortPred, FilterPred>::search_hint(const_pointer ptr) const -> search_hint_type
	{
		if (!m_partition_by_selection)
			return base_type::search_hint(ptr);

		auto begIt = m_store.begin();
		auto endIt = m_store.end();
		
		bool asc = m_partition_by_selection_asc;
		decltype(begIt) pp;
		if (asc)
		{
			auto pred = [this](const_pointer ptr) { return m_selection_set.count(ptr) != 0; };
			pp = std::partition_point(begIt, endIt, pred);
		}
		else
		{
			auto pred = [this](const_pointer ptr) { return m_selection_set.count(ptr) == 0; };
			pp = std::partition_point(begIt, endIt, pred);
		}

		// asc | sel -> action
		//  0     0  -> search [begIt; pp)
		//  0     1  -> search [pp; endIt)
		//  1     0  -> search [pp; endIt)
		//  1     1  -> search [begIt; pp)
		if ((m_selection_set.count(ptr) != 0) ^ asc)
			begIt = pp;
		else
			endIt = pp;

		return {begIt, endIt};
	}
}
