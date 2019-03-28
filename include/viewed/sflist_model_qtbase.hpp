#pragma once
#include <memory>
#include <vector>
#include <algorithm>

#include <ext/config.hpp>
#include <ext/utility.hpp>
#include <ext/try_reserve.hpp>
#include <ext/iterator/zip_iterator.hpp>
#include <ext/range/range_traits.hpp>

#include <viewed/algorithm.hpp>
#include <viewed/forward_types.hpp>
#include <viewed/get_functor.hpp>
#include <viewed/qt_model.hpp>

#include <varalgo/sorting_algo.hpp>
#include <varalgo/on_sorted_algo.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtTools/ToolsBase.hpp>

#if BOOST_COMP_GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif


namespace viewed
{
	/// This class provides base for building qt standalone qt models(holds data internally, not a view to some other container/model).
	/// This is simple list(or table) like model.
	///
	/// It implements complex stuff:
	/// * internal data storage/management;
	/// * sorting/filtering;
	/// * QAbstractItemModel stuff: index calculation, persistent index management on updates and sorting/filtering;
	///
	/// Because this model can be filtered/sorted, it does not provide insert methods(you can not just insert data at any position, it will break sorting order).
	/// There are still assign and append methods; append actually just adds data to model, and then it's placed where sorter says it must be.
	template <class Type, class Sorter, class Filter>
	class sflist_model_qtbase
	{
		using self_type = sflist_model_qtbase;

	public:
		using value_type = Type;
		using const_reference = const value_type &;
		using const_pointer   = const value_type *;

		using reference = const_reference;
		using pointer   = const_pointer;

	public:
		using sort_pred_type   = Sorter;
		using filter_pred_type = Filter;

	protected:
		using model_type = AbstractItemModel;
		using int_vector = std::vector<int>;
		using int_vector_iterator = int_vector::iterator;

	protected:
		using value_container = std::vector<value_type>;
		using value_container_iterator = typename value_container::iterator;

		using search_hint_type = std::pair<
			typename value_container::const_iterator,
			typename value_container::const_iterator
		>;

	public:
		using const_iterator         = typename value_container::const_iterator;
		using const_reverse_iterator = typename value_container::const_reverse_iterator;

		using iterator = const_iterator;
		using reverse_iterator = const_reverse_iterator;

		using size_type       = typename value_container::size_type;
		using difference_type = typename value_container::difference_type;

	protected:
		// Because we hold data - we manage their life-time,
		// and when some values are filtered out we can't just delete them - they will be lost completely.
		// Instead we define visible part and shadow part.
		// Container is partitioned is such way that first comes visible elements, after them shadowed - those who does not pass filter criteria.
		// Whenever filter criteria changes, or elements are changed - elements are moved to/from shadow/visible part according to changes.
		value_container m_store;
		size_type m_nvisible = 0;

		sort_pred_type m_sort_pred;
		filter_pred_type m_filter_pred;

	protected:
		/// acquires pointer to qt model, normally you would inherit both QAbstractItemModel and this class.
		/// default implementation uses dynamic_cast
		virtual model_type * get_model();
		/// emits qt signal model->dataChanged about changed rows. Changed rows are defined by [first; last)
		/// default implantation just calls get_model->dataChanged(index(row, 0), index(row, model->columnCount)
		virtual void emit_changed(int_vector::const_iterator first, int_vector::const_iterator last);
		/// changes persistent indexes via get_model->changePersistentIndex.
		/// [first; last) - range where range[oldIdx - offset] => newIdx.
		/// if newIdx < 0 - index should be removed(changed on invalid, qt supports it)
		virtual void change_indexes(int_vector::const_iterator first, int_vector::const_iterator last, int offset);

	protected:
		/// merges [middle, last) into [first, last) according to m_sort_pred, stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		virtual void merge_newdata(
			value_container_iterator first, value_container_iterator middle, value_container_iterator last,
			bool resort_old = true);

		/// merges [middle, last) into [first, last) according to m_sort_pred, stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		///
		/// range [ifirst, imiddle, ilast) must be permuted the same way as range [first, middle, last)
		virtual void merge_newdata(
			value_container_iterator first, value_container_iterator middle, value_container_iterator last,
			int_vector::iterator ifirst, int_vector::iterator imiddle, int_vector::iterator ilast,
			bool resort_old = true);


		/// sorts [first; last) with m_sort_pred, stable sort
		virtual void stable_sort(value_container_iterator first, value_container_iterator last);
		/// sorts [first; last) with m_sort_pred, stable sort
		/// range [ifirst; ilast) must be permuted the same way as range [first; last)
		virtual void stable_sort(value_container_iterator first, value_container_iterator last,
		                         int_vector::iterator ifirst, int_vector::iterator ilast);

		/// sorts m_store with m_sort_pred, stable sort
		/// emits qt layoutAboutToBeChanged(..., VerticalSortHint), layoutUpdated(..., VerticalSortHint)
		virtual void sort_and_notify();
		/// get pair of iterators that hints where to search element
		virtual search_hint_type search_hint(const_pointer ptr) const;

		/// refilters m_store with m_filter_pred according to rtype:
		/// * same        - does nothing and immediately returns(does not emit any qt signals)
		/// * incremental - calls refilter_full_and_notify
		/// * full        - calls refilter_incremental_and_notify
		virtual void refilter_and_notify(refilter_type rtype);
		/// removes elements not passing m_filter_pred from m_store
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_incremental_and_notify();
		/// completely refilters m_store with values passing m_filter_pred and sorts them according to m_sort_pred
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_full_and_notify();

	public:
		/// container interface
		const_iterator begin()  const noexcept { return m_store.begin(); }
		const_iterator end()    const noexcept { return m_store.begin() + m_nvisible; }
		const_iterator cbegin() const noexcept { return m_store.cbegin(); }
		const_iterator cend()   const noexcept { return m_store.begin() + m_nvisible; }

		const_reverse_iterator rbegin()  const noexcept { return m_store.rbegin() + (m_store.size() - m_nvisible); }
		const_reverse_iterator rend()    const noexcept { return m_store.rend(); }
		const_reverse_iterator crbegin() const noexcept { return m_store.crbegin() + (m_store.size() - m_nvisible); }
		const_reverse_iterator crend()   const noexcept { return m_store.crend(); }

		const_reference at(size_type idx) const { return m_store.at(idx); }
		const_reference operator [](size_type idx) const noexcept { return m_store.operator[](idx); }
		const_reference front() const { return m_store.front(); }
		const_reference back()  const { return m_store[m_nvisible - 1]; }

		size_type size() const noexcept { return m_nvisible; }
		bool empty()     const noexcept { return m_nvisible; }

	public:
		const auto & sort_pred()   const { return m_sort_pred; }
		const auto & filter_pred() const { return m_filter_pred; }

		template <class ... Args> auto filter_by(Args && ... args) -> refilter_type;
		template <class ... Args> void sort_by(Args && ... args);

	public:
		/// erases all elements
		void clear();

		/// erases elements [first, last)
		/// [first, last) must be a valid range
		const_iterator erase(const_iterator first, const_iterator last);
		/// erase element pointed by it
		const_iterator erase(const_iterator it) { return erase(it, std::next(it)); }

		/// appends new record from [first, last)
		template <class SinglePassIterator>
		auto append(SinglePassIterator first, SinglePassIterator last) -> std::enable_if_t<ext::is_iterator_v<SinglePassIterator>>;

		/// clear container and assigns elements from [first, last)
		template <class SinglePassIterator>
		auto assign(SinglePassIterator first, SinglePassIterator last) -> std::enable_if_t<ext::is_iterator_v<SinglePassIterator>>;


		template <class SinglePassRange>
		auto append(SinglePassRange && range) -> std::enable_if_t<std::is_convertible_v<ext::range_value_t<SinglePassRange>, value_type>>
		{ return append(boost::begin(range), boost::end(range)); }

		template <class SinglePassRange>
		auto assign(SinglePassRange && range) -> std::enable_if_t<std::is_convertible_v<ext::range_value_t<SinglePassRange>, value_type>>
		{ return assign(boost::begin(range), boost::end(range)); }

		template <class Arg> auto append(Arg && arg) -> std::enable_if_t<std::is_convertible_v<Arg, value_type>> { append(&arg, &arg + 1); }
		template <class Arg> void push_back(Arg && arg) { return append(std::forward<Arg>(arg)); }

	public:
		virtual ~sflist_model_qtbase() = default;
	};



	template <class Type, class Sorter, class Filter>
	auto sflist_model_qtbase<Type, Sorter, Filter>::get_model() -> model_type *
	{
		auto * model = dynamic_cast<QAbstractItemModel *>(this);
		assert(model);
		return static_cast<model_type *>(model);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::emit_changed(int_vector::const_iterator first, int_vector::const_iterator last)
	{
		auto * model = get_model();
		viewed::emit_changed(model, first, last);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::change_indexes(int_vector::const_iterator first, int_vector::const_iterator last, int offset)
	{
		auto * model = get_model();
		viewed::change_indexes(model, first, last, offset);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::merge_newdata(
			value_container_iterator first, value_container_iterator middle, value_container_iterator last, bool resort_old /*= true*/)
	{
		if (not active(m_sort_pred)) return;

		auto comp = std::cref(m_sort_pred);

		if (resort_old) varalgo::stable_sort(first, middle, comp);
		varalgo::sort(middle, last, comp);
		varalgo::inplace_merge(first, middle, last, comp);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::merge_newdata(
			value_container_iterator first, value_container_iterator middle, value_container_iterator last,
			int_vector_iterator ifirst, int_vector_iterator imiddle, int_vector_iterator ilast,
			bool resort_old /* = true */)
	{
		if (not active(m_sort_pred)) return;

		assert(last - first == ilast - ifirst);
		assert(middle - first == imiddle - ifirst);

		auto comp = viewed::make_get_functor<0>(std::cref(m_sort_pred));

		auto zfirst  = ext::make_zip_iterator(first, ifirst);
		auto zmiddle = ext::make_zip_iterator(middle, imiddle);
		auto zlast   = ext::make_zip_iterator(last, ilast);

		if (resort_old) varalgo::stable_sort(zfirst, zmiddle, comp);
		varalgo::sort(zmiddle, zlast, comp);
		varalgo::inplace_merge(zfirst, zmiddle, zlast, comp);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::stable_sort(value_container_iterator first, value_container_iterator last)
	{
		if (not active(m_sort_pred)) return;

		auto comp = std::cref(m_sort_pred);
		varalgo::stable_sort(first, last, comp);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::stable_sort(
			value_container_iterator first, value_container_iterator last,
			int_vector_iterator ifirst, int_vector_iterator ilast)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_get_functor<0>(std::cref(m_sort_pred));

		auto zfirst = ext::make_zip_iterator(first, ifirst);
		auto zlast = ext::make_zip_iterator(last, ilast);
		varalgo::stable_sort(zfirst, zlast, comp);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::sort_and_notify()
	{
		if (not active(m_sort_pred)) return;

		constexpr int offset = 0;
		int_vector indexes;

		auto first = m_store.begin();
		auto last  = m_store.end();

		indexes.resize(m_store.size());
		auto ifirst = indexes.begin();
		auto ilast = indexes.end();
		std::iota(ifirst, ilast, offset);

		// resort only visible part
		last = first + m_nvisible;
		ilast = ifirst + m_nvisible;

		auto * model = get_model();
		Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model->VerticalSortHint);

		stable_sort(first, last, ifirst, ilast);

		viewed::inverse_index_array(ifirst, ilast, offset);
		change_indexes(ifirst, ilast, offset);

		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->VerticalSortHint);
	}

	template <class Type, class Sorter, class Filter>
	auto sflist_model_qtbase<Type, Sorter, Filter>::search_hint(const_pointer ptr) const -> search_hint_type
	{
		if (not active(m_sort_pred)) return {m_store.begin(), m_store.end()};

		auto first = m_store.begin();
		auto last  = first + m_nvisible;

		return varalgo::equal_range(first, last, *ptr, m_sort_pred);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::refilter_and_notify(refilter_type rtype)
	{
		switch (rtype)
		{
			default:
			case refilter_type::same:        return;

			case refilter_type::incremental: return refilter_incremental_and_notify();
			case refilter_type::full:        return refilter_full_and_notify();
		}
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::refilter_incremental_and_notify()
	{
		if (not active(m_filter_pred)) return;

		auto * model = get_model();
		model->layoutAboutToBeChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);

		// implementation is similar to refilter_full_and_notify,
		// but more simple - only visible area should filtered, and no sorting should be done
		// see refilter_full_and_notify - for more description
		constexpr int offset = 0;
		int_vector index_array;
		index_array.resize(m_store.size());

		auto fpred  = std::cref(m_filter_pred);
		auto zfpred = viewed::make_get_functor<0>(fpred);

		auto vfirst = m_store.begin();
		auto vlast  = vfirst + m_nvisible;

		auto ivfirst = index_array.begin();
		auto ivlast  = ivfirst + m_nvisible;
		auto isfirst = ivlast;
		auto islast  = index_array.end();

		std::iota(ivfirst, islast, offset);

		auto[vpp, ivpp] = std::stable_partition(
			ext::make_zip_iterator(vfirst, ivfirst),
			ext::make_zip_iterator(vlast, ivlast),
			zfpred).get_iterator_tuple();

		std::transform(ivpp, ivlast, ivpp, viewed::mark_index);

		m_nvisible = vpp - vfirst;

		// recalculate qt persistent indexes and notify any clients
		viewed::inverse_index_array(index_array.begin(), index_array.end(), offset);
		change_indexes(index_array.begin(), index_array.end(), offset);

		model->layoutChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::refilter_full_and_notify()
	{
		if (not active(m_filter_pred) and m_nvisible == m_store.size()) return;

		auto * model = get_model();
		model->layoutAboutToBeChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);

		// We must rearrange children according to sorting/filtering criteria.
		// Note when working with visible area - order of visible elements should not changed - we want stability.
		// Also Qt persistent indexes should be recalculated.
		//
		// layout of elements at start:
		//
		// |vfirst                     |vlast                      |m_store.end()
		// ---------------------------------------------------------
		// |    visible elements       |      shadow elements      |
		// ---------------------------------------------------------
		// |m_store.begin()            |sfirst                     |slast
		//
		constexpr int offset = 0;
		int nvisible_new;
		int_vector index_array;
		index_array.resize(m_store.size());

		auto fpred  = std::cref(m_filter_pred);
		auto zfpred = viewed::make_get_functor<0>(fpred);

		auto vfirst = m_store.begin();
		auto vlast  = vfirst + m_nvisible;
		auto sfirst = vlast;
		auto slast  = m_store.end();

		// elements index array - it will be permutated with elements array, later it will be used for recalculating qt indexes
		auto ivfirst = index_array.begin();
		auto ivlast  = ivfirst + m_nvisible;
		auto isfirst = ivlast;
		auto islast  = index_array.end();
		std::iota(ivfirst, islast, offset);

		if (not viewed::active(m_filter_pred))
		{
			// if there is no filter - just merge shadow area with visible
			nvisible_new = slast - vfirst;
			merge_newdata(vfirst, vlast, slast, ivfirst, ivlast, islast, false);
		}
		else
		{
			// partition visible area against filter predicate
			auto [vpp, ivpp] = std::stable_partition(
				ext::make_zip_iterator(vfirst, ivfirst),
				ext::make_zip_iterator(vlast, ivlast),
				zfpred).get_iterator_tuple();

			// partition shadow area against filter predicate
			auto [spp, ispp] = std::partition(
				ext::make_zip_iterator(sfirst, isfirst),
				ext::make_zip_iterator(slast, islast),
				zfpred).get_iterator_tuple();

			// mark indexes if elements that do not pass filtering as removed, to outside world they are removed
			std::transform(ivpp, ivlast, ivpp, viewed::mark_index);
			std::transform(ispp, islast, ispp, viewed::mark_index);

			// current layout of elements:
			// P - passes filter, X - not passes filter
			// |vfirst                 |vlast
			// -------------------------------------------------
			// |P|P|P|P|P|P|X|X|X|X|X|X|P|P|P|P|P|X|X|X|X|X|X|X|
			// -------------------------------------------------
			// |           |           |sfirst   |             |slast
			//             |vpp                  |spp

			// rotate [sfirst; spp) at the the at of visible area, that passes filter
			vlast  = std::rotate(vpp, sfirst, spp);
			ivlast = std::rotate(ivpp, isfirst, ispp);
			nvisible_new = vlast - vfirst;
			// merge new elements from shadow area
			merge_newdata(vfirst, vpp, vlast, ivfirst, ivpp, ivlast, false);
		}

		// rearranging is over -> set order in boost::multi_index_container
		m_nvisible = nvisible_new;

		// recalculate qt persistent indexes and notify any clients
		viewed::inverse_index_array(index_array.begin(), index_array.end(), offset);
		change_indexes(index_array.begin(), index_array.end(), offset);

		model->layoutChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);
	}

	template <class Type, class Sorter, class Filter>
	template <class ... Args>
	auto sflist_model_qtbase<Type, Sorter, Filter>::filter_by(Args && ... args) -> refilter_type
	{
		auto rtype = m_filter_pred.set_expr(std::forward<Args>(args)...);
		refilter_and_notify(rtype);

		return rtype;
	}

	template <class Type, class Sorter, class Filter>
	template <class ... Args>
	void sflist_model_qtbase<Type, Sorter, Filter>::sort_by(Args && ... args)
	{
		m_sort_pred = sort_pred_type(std::forward<Args>(args)...);
		sort_and_notify();
	}

	template <class Type, class Sorter, class Filter>
	void sflist_model_qtbase<Type, Sorter, Filter>::clear()
	{
		auto * model = get_model();
		model->beginResetModel();
		m_store.clear();
		m_nvisible = 0;
		model->endResetModel();
	}

	template <class Type, class Sorter, class Filter>
	auto sflist_model_qtbase<Type, Sorter, Filter>::erase(const_iterator first, const_iterator last) -> const_iterator
	{
		if (first == last) return first;

		assert(first <= last);
		assert(m_store.begin() <= first and last <= m_store.end());

		int first_pos = first - m_store.begin();
		int last_pos  = last  - m_store.begin();

		auto * model = get_model();
		model->beginRemoveRows(model_type::invalid_index, first_pos, last_pos - 1);
		auto ret = m_store.erase(first, last);
		m_nvisible -= std::max<int>(0, m_nvisible - std::min<size_type>(last_pos, m_nvisible));
		model->endRemoveRows();

		return ret;
	}

	template <class Type, class Sorter, class Filter>
	template <class SinglePassIterator>
	auto sflist_model_qtbase<Type, Sorter, Filter>::assign(SinglePassIterator first, SinglePassIterator last) -> std::enable_if_t<ext::is_iterator_v<SinglePassIterator>>
	{
		auto * model = get_model();
		model->beginResetModel();

		m_nvisible = 0;
		m_store.assign(first, last);

		auto vfirst = m_store.begin();
		auto vlast  = m_store.end();
		auto sfirst = vlast;
		auto slast  = vlast;

		if (viewed::active(m_filter_pred))
			vlast = sfirst = std::partition(vfirst, vlast, std::cref(m_filter_pred));

		stable_sort(vfirst, vlast);
		m_nvisible = vlast - vfirst;

		model->endResetModel();
	}

	template <class Type, class Sorter, class Filter>
	template <class SinglePassIterator>
	std::enable_if_t<ext::is_iterator_v<SinglePassIterator>> sflist_model_qtbase<Type, Sorter, Filter>::append(SinglePassIterator first, SinglePassIterator last) //-> std::enable_if_t<ext::is_iterator_v<SinglePassIterator>>
	{
		if (first == last) return;

		auto * model = get_model();
		model->layoutAboutToBeChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);

		auto old_size = m_store.size();
		// append elements
		m_store.insert(m_store.end(), first, last);

		// We must rearrange children according to sorting/filtering criteria.
		//  * some elements have been inserted - those should placed in visible or shadow area if they do not pass filtering
		//  * when working with visible area - order of visible elements should not changed - we want stability
		// And Qt persistent indexes should be recalculated.
		//
		// layout of elements at start:
		//
		// |vfirst                     |vlast                      |nfirst              |nlast
		// ------------------------------------------------------------------------------
		// |    visible elements       |      shadow elements      |    new elements    |
		// ------------------------------------------------------------------------------
		// |m_store.begin()            |sfirst                     |slast               |m_store.end()
		//

		constexpr int offset = 0;
		auto inserted_count = m_store.size() - old_size;
		auto nvisible_new = m_nvisible + inserted_count;
		auto fpred = std::cref(m_filter_pred);

		auto vfirst = m_store.begin();
		auto vlast  = vfirst + m_nvisible;
		auto sfirst = vlast;
		auto slast  = vfirst + old_size;
		auto nfirst = slast;
		auto nlast  = m_store.end();

		// elements index array - it will be permutated with elements array, later it will be used for recalculating qt indexes
		int_vector index_array;
		index_array.resize(m_store.size());
		auto ifirst  = index_array.begin();
		auto imiddle = ifirst + m_nvisible;
		auto ilast   = index_array.end();
		std::iota(ifirst, ilast, offset); // at start: elements are placed in natural order: [0, 1, 2, ...]

		if (viewed::active(m_filter_pred))
		{
			// elements passing filtering at end of new area, and then rotate them after visible area
			auto npp = std::partition(nfirst, nlast, std::not_fn(fpred));
			nvisible_new = (vlast - vfirst) + (nlast - npp);

			// rotate [npp, nlast) at the beginning of shadow area
			// and in fact merge those with visible area
			nlast = std::rotate(sfirst, npp, nlast);
		}

		// if some elements from visible area are changed and they still in the visible area - we need to resort them => resort whole visible area.
		constexpr bool resort_old = false;
		// resort visible area, merge new elements and changed from shadow area(std::stable sort + std::inplace_merge)
		merge_newdata(vfirst, vlast, nlast, ifirst, imiddle, ifirst + (nlast - vfirst), resort_old);

		// and erase removed elements
		m_nvisible = nvisible_new;

		// recalculate qt persistent indexes and notify any clients
		viewed::inverse_index_array(ifirst, ilast, offset);
		change_indexes(index_array.begin(), index_array.end(), offset);

		model->layoutChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);
	}

} // namespace viewed

#if BOOST_COMP_GNUC
#pragma GCC diagnostic pop
#endif
