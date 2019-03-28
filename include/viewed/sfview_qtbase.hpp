#pragma once
#include <vector>
#include <algorithm>

#include <viewed/forward_types.hpp>
#include <viewed/algorithm.hpp>
#include <viewed/get_functor.hpp>
#include <viewed/indirect_functor.hpp>
#include <viewed/view_qtbase.hpp>

#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <ext/algorithm.hpp>
#include <ext/iterator/zip_iterator.hpp>

#include <varalgo/sorting_algo.hpp>
#include <varalgo/on_sorted_algo.hpp>

namespace viewed
{
	/// see also view_qtbase description for more information
	/// 
	/// sfview_qtbase is sorted and filtered based on provided SortPred, FilterPred.
	/// Those can be: simple predicate or std::variant of different predicates.
	/// 
	/// predicates can, optionally, be convertible to bool: static_cast<bool>(pred) == true ->
	/// predicate is active, if conversion is not provided - predicate is always active.
	///   sort_pred - predicate can sort items, otherwise it can not and whole view will be unsorted.
	///   filter_pred - predicate can filter items, otherwise it is assumed all items are always passes filter(empty filter)
	/// 
	/// In derived class you can provide methods like sort_by/filter_by,
	/// which will configure those predicates
	/// 
	/// @Param Container - class to which this view will connect and listen updates, see view_qtbase for more description
	/// @Param SortPred - sort predicate or std::variant of predicates,
	///                   example: std::less<Type>, std::variant<std::less<Type>, std::greater<Type>>
	/// @Param FilterPred - filter predicate or std::variant of predicates,
	///                     NOTE: std::variant is not supported yet for filter predicate
	template <
		class Container,
		class SortPred,
		class FilterPred
	>
	class sfview_qtbase : public view_qtbase<Container>
	{
		typedef view_qtbase<Container> base_type;
		typedef sfview_qtbase<Container, SortPred, FilterPred> self_type;

	public:
		using typename base_type::container_type;
		using typename base_type::view_pointer_type;

		typedef SortPred sort_pred_type;
		typedef FilterPred filter_pred_type;

		typedef viewed::refilter_type refilter_type;

	protected:
		using typename base_type::store_type;
		using typename base_type::signal_range_type;
		using typename base_type::model_type;
		using typename base_type::int_vector;
		using int_vector_iterator = typename int_vector::iterator;

		using signal_const_iterator = typename signal_range_type::const_iterator;

		using base_type::m_owner;
		using base_type::m_store;
		using base_type::get_model;
		using base_type::get_view_pointer;
		using base_type::change_indexes;
		using base_type::emit_changed;


		typedef typename store_type::iterator       store_iterator;
		typedef typename store_type::const_iterator store_const_iterator;

		typedef std::pair <
			store_const_iterator,
			store_const_iterator
		> search_hint_type;

	protected:
		sort_pred_type m_sort_pred;
		filter_pred_type m_filter_pred;

	public:
		/// reinitializes view from owner
		virtual void reinit_view() override;

	protected:
		/// adjusts view with erased/updated/inserted data, preserving filter/sort order. stable
		/// emits appropriate qt signals, uses merge_newdata(iter..., iter..., ...) to calculate index permutations.
		virtual void update_data(
			const signal_range_type & sorted_erased,
			const signal_range_type & sorted_updated,
			const signal_range_type & inserted) override;
		
	protected:
		/// adjusts view with erased/updated/inserted data, preserving filter/sort order. stable,
		/// new and updated should already be appended to m_store in order: current data..., updated..., inserted...].
		/// emits appropriate qt signals, uses merge_newdata(iter..., iter..., ...) to calculate index permutations.
		virtual void update_store(
			store_iterator first, store_iterator first_updated,
			store_iterator first_inserted, store_iterator last,
			signal_const_iterator first_erased, signal_const_iterator last_erased);

		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		virtual void merge_newdata(
			store_iterator first, store_iterator middle, store_iterator last,
			bool resort_old = true);

		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		/// 
		/// range [ifirst, imiddle, ilast) must be permuted the same way as range [first, middle, last)
		virtual void merge_newdata(
			store_iterator first, store_iterator middle, store_iterator last,
		    int_vector_iterator ifirst, int_vector_iterator imiddle, int_vector_iterator ilast,
			bool resort_old = true);

		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		virtual void stable_sort(store_iterator first, store_iterator last);
		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		/// range [ifirst; ilast) must be permuted the same way as range [first; last)
		virtual void stable_sort(store_iterator first, store_iterator last,
		                         int_vector_iterator ifirst, int_vector_iterator ilast);

		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		/// emits qt layoutAboutToBeChanged(..., VerticalSortHint), layoutUpdated(..., VerticalSortHint)
		virtual void sort_and_notify(store_iterator first, store_iterator last);

		/// get pair of iterators that hints where to search element
		virtual search_hint_type search_hint(view_pointer_type ptr) const;

		/// refilters m_store with m_filter_pred according to rtype:
		/// * same        - does nothing and immediately returns(does not emit any qt signals)
		/// * incremental - calls refilter_full_and_notify
		/// * full        - calls refilter_incremental_and_notify
		virtual void refilter_and_notify(refilter_type rtype);
		/// removes elements not passing m_filter_pred from m_store
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_incremental_and_notify();
		/// fills m_store from owner with values passing m_filter_pred and sorts them according to m_sort_pred
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_full_and_notify();

	public:
		const auto & sort_pred()   const { return m_sort_pred; }
		const auto & filter_pred() const { return m_filter_pred; }

		template <class ... Args> auto filter_by(Args && ... args) -> refilter_type;
		template <class ... Args> void sort_by(Args && ... args);

	protected:
		sfview_qtbase(ext::noinit_type noinit, std::shared_ptr<container_type> owner,
		              sort_pred_type sort_pred = {},
		              filter_pred_type filter_pred = {})
			: base_type(noinit, std::move(owner)),
		      m_sort_pred(std::move(sort_pred)),
		      m_filter_pred(std::move(filter_pred))
		{ }

	public:
		sfview_qtbase(std::shared_ptr<container_type> owner,
		              sort_pred_type sort_pred = {},
		              filter_pred_type filter_pred = {})
		    : sfview_qtbase(ext::noinit, std::move(owner), std::move(sort_pred), std::move(filter_pred))
		{ this->init(); }

		virtual ~sfview_qtbase() = default;

		sfview_qtbase(const sfview_qtbase &) = delete;
		sfview_qtbase & operator =(const sfview_qtbase &) = delete;

		sfview_qtbase(sfview_qtbase &&) = delete;
		sfview_qtbase & operator =(sfview_qtbase &&) = delete;

	}; //class sfview_qtbase

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::reinit_view()
	{
		auto range = *m_owner | boost::adaptors::transformed(get_view_pointer);
		if (not active(m_filter_pred))
			m_store.assign(range.begin(), range.end());
		else
		{
			m_store.clear();
			auto pred = viewed::make_indirect_functor(m_filter_pred);
			std::copy_if(range.begin(), range.end(), std::back_inserter(m_store), pred);
		}

		stable_sort(m_store.begin(), m_store.end());
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::update_data(
		const signal_range_type & sorted_erased,
		const signal_range_type & sorted_updated,
		const signal_range_type & inserted)
	{
		if (not active(m_sort_pred) and not active(m_filter_pred))
			base_type::update_data(sorted_erased, sorted_updated, inserted);
		else
		{
			boost::push_back(m_store, sorted_updated);
			boost::push_back(m_store, inserted);

			auto first = m_store.begin();
			auto last = m_store.end();
			auto first_inserted = last - inserted.size();
			auto first_updated = first_inserted - sorted_updated.size();

			update_store(first, first_updated, first_inserted, last,
			             sorted_erased.begin(), sorted_erased.end());
		}
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::update_store(
		store_iterator first, store_iterator first_updated,
		store_iterator first_inserted, store_iterator last,
		signal_const_iterator first_erased, signal_const_iterator last_erased)
	{
		// inserted records can be appended(only those passing filter predicate) to m_store and then merged into via inplace_merge algorithm.
		// With updated it's more complicated. for every updated record it's filtered state can be changed:
		//
		// * passes -> not passes - items should be erased from m_store
		// * not passes -> passes - those items should threated like inserted
		// * passes -> passes
		//     it's position in m_store may or may not changed, we do not have a way to know it - record must be resorted.
		//     Also remember we are stable sorted, we can't erase and then re-merge it:
		//     if it's sort criteria doesn't changed, and there are equal neighbors -
		//     it's position will change => we are unstable.
		//     
		//     Those records should be left where they where and stable_sort should called on entire m_store.
		//     
		// We do not know, in advance, status of updated records.
		// Algorithm is next:
		// We append all updated records to m_store. Now we have:
		// [first, middle) - current records;
		// [middle, last) - updated candidates, sorted by pointer value;
		//
		// every item from [first, middle) is searched in [middle, last), found items from [middle, last) are marked via setting lowest bit to 1
		// (because of heap allocation those pointer lowest bit will always be 0)
		// complexity is: N * log M
		//
		// found items are tested if they pass filter predicate, and if not - they are removed via remove_if.
		// complexity is: N
		// 
		// marked items from [middle, last) are removed as they are already present in [first, middle)
		// complexity is: M
		// 
		// left updated and inserted than merged to m_store, also merge operation permutates index_array,
		// which is used to update qt persistent indexes

		// this case(both not active) is handled by update_data(forwarded to view_qtbase implementation)
		assert(active(m_sort_pred) or active(m_filter_pred));

		// currently just assume first always is beginning of m_store
		assert(first == m_store.begin());
		// updated part must be sorted by pointer addr
		assert(std::is_sorted(first_updated, first_inserted));
		assert(std::is_sorted(first_erased, last_erased));

		auto     fpred = [this](auto ptr) { return m_filter_pred(*ptr); };
		//auto not_fpred = [this](auto ptr) { return not m_filter_pred(*ptr); };

		int_vector index_array, affected_indexes;
		int_vector_iterator removed_first, removed_last, changed_first, changed_last;
		std::size_t middle_sz = first_updated - first;
		bool order_changed = false;

		auto middle = first_updated;
		auto last_updated = first_inserted;
		auto last_inserted = last;
		
		affected_indexes.resize(first_inserted - first_updated + last_erased - first_erased);
		removed_first = removed_last = affected_indexes.begin();
		changed_first = changed_last = affected_indexes.end();

		if (first_updated == last_updated)
		{
			// if there erased ones - erase them from the store
			for (auto it = first; it != middle; ++it)
			{
				if (std::binary_search(first_erased, last_erased, *it))
					*removed_last++ = static_cast<int>(it - first);
			}

			last = middle = viewed::remove_indexes(first, middle, removed_first, removed_last);
		}
		else // there are updates
		{
			for (auto it = first; it != middle; ++it)
			{
				auto ptr = *it;
				if (std::binary_search(first_erased, last_erased, ptr))
					*removed_last++ = static_cast<int>(it - first);
				else
				{
					auto found = std::lower_bound(first_updated, last_updated, ptr);
					if (found == last_updated) continue;
					if (ptr != *found) continue;

					*found = viewed::mark_pointer(*found);
					int row = static_cast<int>(it - first);
					bool passes = not active(m_filter_pred) or m_filter_pred(*ptr);

					if (passes)   *--changed_first = row;
					else          *removed_last++ = row;
				}
			}

			order_changed = changed_first != changed_last;
			std::reverse(changed_first, changed_last);
			emit_changed(changed_first, changed_last);
			
			middle = viewed::remove_indexes(first, middle, removed_first, removed_last);
			last = std::copy_if(first_updated, last_updated, middle,
			                    [fpred](auto ptr) { return not viewed::marked_pointer(ptr) and fpred(ptr); });
		}

		// copy filter new elements
		if (active(m_filter_pred))
			last = std::copy_if(first_inserted, last_inserted, last, fpred);
		else
			last = std::copy(first_inserted, last_inserted, last);

		// and resize m_store
		middle_sz = middle - first;
		m_store.resize(last - first);


		auto * model = get_model();
		Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model->NoLayoutChangeHint);

		first = m_store.begin();
		middle = first + middle_sz;
		last = m_store.end();

		constexpr int offset = 0;
		index_array.resize(m_store.size() + (removed_last - removed_first));

		auto ifirst = index_array.begin();
		auto ilast = index_array.end();

		std::iota(ifirst, ilast, offset);
		ilast = viewed::remove_indexes(ifirst, ilast, removed_first, removed_last);
		ilast = std::transform(removed_first, removed_last, ilast, viewed::mark_index);

		merge_newdata(
			first, middle, last,
			ifirst, ifirst + middle_sz, ifirst + m_store.size(),
			order_changed
		);

		viewed::inverse_index_array(ifirst, ilast, offset);
		change_indexes(ifirst, ilast, offset);

		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->NoLayoutChangeHint);
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::
		merge_newdata(store_iterator first, store_iterator middle, store_iterator last, bool resort_old /*= true*/)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_indirect_functor(std::cref(m_sort_pred));

		if (resort_old) varalgo::stable_sort(first, middle, comp);
		varalgo::sort(middle, last, comp);
		varalgo::inplace_merge(first, middle, last, comp);
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::
		merge_newdata(store_iterator first, store_iterator middle, store_iterator last,
	                  int_vector_iterator ifirst, int_vector_iterator imiddle, int_vector_iterator ilast,
		              bool resort_old /* = true */)
	{
		if (not active(m_sort_pred)) return;

		assert(last - first == ilast - ifirst);
		assert(middle - first == imiddle - ifirst);

		auto comp = viewed::make_get_functor<0>(viewed::make_indirect_functor(std::cref(m_sort_pred)));

		auto zfirst  = ext::make_zip_iterator(first, ifirst);
		auto zmiddle = ext::make_zip_iterator(middle, imiddle);
		auto zlast   = ext::make_zip_iterator(last, ilast);

		if (resort_old) varalgo::stable_sort(zfirst, zmiddle, comp);
		varalgo::sort(zmiddle, zlast, comp);
		varalgo::inplace_merge(zfirst, zmiddle, zlast, comp);
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::stable_sort(store_iterator first, store_iterator last)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_indirect_functor(std::cref(m_sort_pred));
		varalgo::stable_sort(first, last, comp);
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::
		stable_sort(store_iterator first, store_iterator last,
	         int_vector_iterator ifirst, int_vector_iterator ilast)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_get_functor<0>(viewed::make_indirect_functor(std::cref(m_sort_pred)));

		auto zfirst = ext::make_zip_iterator(first, ifirst);
		auto zlast = ext::make_zip_iterator(last, ilast);
		varalgo::stable_sort(zfirst, zlast, comp);
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::sort_and_notify(store_iterator first, store_iterator last)
	{
		if (not active(m_sort_pred)) return;

		auto * model = get_model();
		Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model->VerticalSortHint);

		int offset = first - m_store.begin();
		int_vector indexes(last - first);
		auto ifirst = indexes.begin();
		auto ilast = indexes.end();
		std::iota(ifirst, ilast, offset);

		stable_sort(first, last, ifirst, ilast);

		viewed::inverse_index_array(ifirst, ilast, offset);
		change_indexes(ifirst, ilast, offset);

		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->VerticalSortHint);
	}

	template <class Container, class SortPred, class FilterPred>
	auto sfview_qtbase<Container, SortPred, FilterPred>::search_hint(view_pointer_type ptr) const -> search_hint_type
	{
		if (not active(m_sort_pred)) return {m_store.begin(), m_store.end()};

		auto comp = viewed::make_indirect_functor(std::cref(m_sort_pred));
		return varalgo::equal_range(m_store, ptr, comp);
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::refilter_and_notify(refilter_type rtype)
	{
		switch (rtype)
		{
			default:
			case refilter_type::same:        return;

			case refilter_type::incremental: return refilter_incremental_and_notify();
			case refilter_type::full:        return refilter_full_and_notify();
		}
	}

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::refilter_incremental_and_notify()
	{
		if (not active(m_filter_pred)) return;
		
		auto test = [this](view_pointer_type ptr) { return !m_filter_pred(*ptr); };

		int_vector affected_indexes(m_store.size());
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

	template <class Container, class SortPred, class FilterPred>
	void sfview_qtbase<Container, SortPred, FilterPred>::refilter_full_and_notify()
	{
		if (not active(m_sort_pred) and not active(m_filter_pred))
			base_type::reinit_view();
		else
		{
			auto sz = m_store.size();
			boost::push_back(m_store, *m_owner | boost::adaptors::transformed(get_view_pointer));

			auto first = m_store.begin();
			auto last = m_store.end();
			auto first_updated = first + sz;
			std::sort(first_updated, last);

			signal_const_iterator noerased {};
			update_store(first, first_updated, last, last, noerased, noerased);
		}
	}

	template <class Container, class SortPred, class FilterPred>
	template <class ... Args>
	auto sfview_qtbase<Container, SortPred, FilterPred>::filter_by(Args && ... args) -> refilter_type
	{
		auto rtype = m_filter_pred.set_expr(std::forward<Args>(args)...);
		refilter_and_notify(rtype);

		return rtype;
	}

	template <class Container, class SortPred, class FilterPred>
	template <class ... Args>
	void sfview_qtbase<Container, SortPred, FilterPred>::sort_by(Args && ... args)
	{
		m_sort_pred = sort_pred_type(std::forward<Args>(args)...);
		sort_and_notify(m_store.begin(), m_store.end());
	}
}
