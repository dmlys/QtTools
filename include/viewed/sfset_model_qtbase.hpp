#pragma once
#include <memory>
#include <vector>
#include <tuple>
#include <algorithm>

#include <ext/config.hpp>
#include <ext/utility.hpp>
#include <ext/try_reserve.hpp>
#include <ext/iterator/zip_iterator.hpp>
#include <ext/iterator/outdirect_iterator.hpp>
#include <ext/iterator/indirect_iterator.hpp>
#include <ext/range/range_traits.hpp>
#include <ext/range/adaptors/moved.hpp>
#include <ext/range/adaptors/outdirected.hpp>

#include <viewed/algorithm.hpp>
#include <viewed/forward_types.hpp>
#include <viewed/get_functor.hpp>
#include <viewed/indirect_functor.hpp>
#include <viewed/qt_model.hpp>

#include <varalgo/sorting_algo.hpp>
#include <varalgo/on_sorted_algo.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtTools/ToolsBase.hpp>

#if BOOST_COMP_GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif



namespace viewed
{
	/// This class provides base for building qt standalone qt models(holds data internally, not a view to some other container/model).
	/// Data is assumed to be unique identified by some key: only one record with given key is present in model at any time.
	///
	/// It implements complex stuff:
	/// * internal data storage/management;
	/// * sorting/filtering;
	/// * QAbstractItemModel stuff: index calculation, persistent index management on updates and sorting/filtering;
	///
	/// Traits - type container describing type and some related types:
	///   value_type          - type this model manages
	///   key_type            - key type
	///   key_equal_to_type   - predicate used to compare key for equality, typically std::equal_to<>
	///   key_less_type       - predicate used to compare key for less, typically std::less<>
	///   key_hash_type       - functor used to calculate hash from key, typically std::hash<key_type>
	///
	///   static auto get_key(const value_type & val) - key_type/const key_type &
	///        returns/extracts key value, usually something like: return val.name
	///
	///   static void update(value_type & curval, value_type && newval)
	///   static void update(value_type & curval, const value_type & newval)
	///        updates current value with new data, usually: curval = std::move(newval);
	///
	///   using sort_pred_type = ...
	///   using filter_pred_type = ...
	///     predicates for sorting/filtering items based on some criteria, this is usually sorting by columns and filtering by some text
	///     should be default constructable
	///
	template <class Traits>
	class sfset_model_qtbase
	{
		using self_type = sfset_model_qtbase;

	public:
		using traits_type = Traits;

	public:
		using value_type      = typename traits_type::value_type;
		using const_reference = const value_type &;
		using const_pointer   = const value_type *;

		using reference = const_reference;
		using pointer   = const_pointer;

	public:
		using key_type          = typename traits_type::key_type;
		using key_equal_to_type = typename traits_type::key_equal_to_type;
		using key_less_type     = typename traits_type::key_less_type;
		using key_hash_type     = typename traits_type::key_hash_type;

		using sort_pred_type   = typename traits_type::sort_pred_type;
		using filter_pred_type = typename traits_type::filter_pred_type;

	protected:
		using value_ptr_vector   = std::vector<const value_type *>;
		using value_ptr_iterator = typename value_ptr_vector::iterator;

		using model_type = AbstractItemModel;
		using int_vector = std::vector<int>;
		using int_vector_iterator = int_vector::iterator;

	public:
		struct get_key_type
		{
			using result_type = std::invoke_result_t<decltype(traits_type::get_key), const value_type &>;
			template <class Arg> decltype(auto) operator()(Arg && arg) const { return traits_type::get_key(std::forward<Arg>(arg)); }

			//using result_type = std::invoke_result_t<get_key_type, const value_type &>;
		};

	protected:
		// boost::multi_index_container is used to index elements by key and by position
		//
		// Because we hold data - we manage their life-time
		// and when some values are filtered out we can't just delete them - they will be lost completly.
		// Instead we define visible part and shadow part.
		// Container is partitioned is such way that first comes visible elements, after them shadowed - those who does not pass filter criteria.
		// Whenever filter criteria changes, or elements are changed - elements are moved to/from shadow/visible part according to changes.

		using value_container = boost::multi_index_container<
			value_type,
			boost::multi_index::indexed_by<
				boost::multi_index::random_access<>,
				boost::multi_index::hashed_unique<get_key_type, key_hash_type, key_equal_to_type>
			>
		>;

		static constexpr unsigned by_seq  = 0;
		static constexpr unsigned by_code = 1;

		using seq_view_type  = typename value_container::template nth_index<by_seq> ::type;
		using code_view_type = typename value_container::template nth_index<by_code>::type;


		using search_hint_type = std::pair<
			typename seq_view_type::const_iterator,
			typename seq_view_type::const_iterator
		>;


		// helper struct - to pass data into rearrange method
		struct upsert_context
		{
			int_vector::iterator
				removed_first, removed_last, // share same array, append by incrementing removed_last
				changed_first, changed_last; //                   append by decrementing changed_first

			// on input - size of container, without newly inserted elements,
			// on output - should be size of container, without elements to be erased
			std::size_t size;

			// on input how much elements are visible, basically a m_nvisible
			// on output how much elements are visible after rearranging elements, including sorted, erased, inserted
			std::size_t * nvisible_ptr;

			value_container  * container;   // container that should be rearranged
			value_ptr_vector * vptr_array;  // pointer to buffer value_ptr_vector, not null
			int_vector       * index_array; // pointer to buffer index vector, not null
		};

	public:
		using const_iterator         = typename seq_view_type::const_iterator;
		using const_reverse_iterator = typename seq_view_type::const_reverse_iterator;

		using iterator = const_iterator;
		using reverse_iterator = const_reverse_iterator;

		using size_type       = typename seq_view_type::size_type;
		using difference_type = typename seq_view_type::difference_type;

	public:
		static constexpr get_key_type get_key {};
		static constexpr auto make_ref = [](auto * ptr) { return std::ref(*ptr); };

	protected:
		value_container m_store;
		size_type m_nvisible = 0;

		filter_pred_type m_filter_pred;
		sort_pred_type m_sort_pred;

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
		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		virtual void merge_newdata(
			value_ptr_iterator first, value_ptr_iterator middle, value_ptr_iterator last,
			bool resort_old = true);

		/// merges m_store's [middle, last) into [first, last) according to m_sort_pred. stable.
		/// first, middle, last - is are one range, as in std::inplace_merge
		/// if resort_old is true it also resorts [first, middle), otherwise it's assumed it's sorted
		///
		/// range [ifirst, imiddle, ilast) must be permuted the same way as range [first, middle, last)
		virtual void merge_newdata(
			value_ptr_iterator first, value_ptr_iterator middle, value_ptr_iterator last,
			int_vector::iterator ifirst, int_vector::iterator imiddle, int_vector::iterator ilast,
			bool resort_old = true);


		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		virtual void stable_sort(value_ptr_iterator first, value_ptr_iterator last);
		/// sorts m_store's [first; last) with m_sort_pred, stable sort
		/// range [ifirst; ilast) must be permuted the same way as range [first; last)
		virtual void stable_sort(value_ptr_iterator first, value_ptr_iterator last,
		                         int_vector::iterator ifirst, int_vector::iterator ilast);

		/// sorts m_store's [first; last) with m_sort_pred, stable sort
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
		/// fills m_store from owner with values passing m_filter_pred and sorts them according to m_sort_pred
		/// emits qt layoutAboutToBeChanged(..., NoLayoutChangeHint), layoutUpdated(..., NoLayoutChangeHint)
		virtual void refilter_full_and_notify();

		/// rearranges elements in ctx.container according to input data from ctx.
		/// should be called after some elements where inserted, some changed, and some are about to be erased.
		/// does not do actual erasing, but rotates elements that should be removed at the end of ctx.container
		virtual void rearrange_and_notify(upsert_context & ctx);

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
		/// erases elements [first, last) from internal store and views
		/// [first, last) must be a valid range
		const_iterator erase(const_iterator first, const_iterator last);
		/// erase element pointed by it
		const_iterator erase(const_iterator it) { return erase(it, std::next(it)); }
		/// erase element by key
		template <class CompatibleKey>
		size_type erase(const CompatibleKey & key);

		template <class SinglePassIterator>
		auto erase(SinglePassIterator first, SinglePassIterator last)
			-> std::enable_if_t<std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, key_type>, size_type>;

		template <class SinglePassRange>
		auto erase(SinglePassRange && range)
			-> std::enable_if_t<std::is_convertible_v<ext::range_value_t<SinglePassRange>, key_type>, size_type>
		{ using std::begin; using std::end; return erase(begin(range), end(range)); }


		/// clear container and assigns elements from [first, last)
		template <class SinglePassIterator>
		void assign(SinglePassIterator first, SinglePassIterator last);

		/// upserts new record from [first, last)
		/// records which are already in container will be replaced with new ones
		template <class SinglePassIterator>
		void upsert(SinglePassIterator first, SinglePassIterator last);


		template <class Range>
		auto upsert(Range && range) -> std::enable_if_t<ext::is_range_v<ext::remove_cvref_t<Range>>>
		{ using std::begin; using std::end; upsert(begin(range), end(range)); }

		template <class Range>
		auto assign(Range && range) -> std::enable_if_t<ext::is_range_v<ext::remove_cvref_t<Range>>>
		{ using std::begin; using std::end; assign(begin(range), end(range)); }

		template <class Arg> void append(Arg && arg) { append(&arg, &arg + 1); }
		template <class Arg> void push_back(Arg && arg) { return append(std::forward<Arg>(arg)); }

	public:
		virtual ~sfset_model_qtbase() = default;
	};


	template <class Traits>
	auto sfset_model_qtbase<Traits>::get_model() -> model_type *
	{
		auto * model = dynamic_cast<QAbstractItemModel *>(this);
		assert(model);
		return static_cast<model_type *>(model);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::emit_changed(int_vector::const_iterator first, int_vector::const_iterator last)
	{
		if (first == last) return;

		auto * model = get_model();
		int ncols = model->columnCount(model_type::invalid_index);

		for (; first != last; ++first)
		{
			// lower index on top, higher on bottom
			int top, bottom;
			top = bottom = *first;

			// try to find the sequences with step of 1, for example: ..., 4, 5, 6, ...
			for (++first; first != last and *first - bottom == 1; ++first, ++bottom)
				continue;

			--first;

			auto top_left = model->index(top, 0, model_type::invalid_index);
			auto bottom_right = model->index(bottom, ncols - 1, model_type::invalid_index);
			Q_EMIT model->dataChanged(top_left, bottom_right, model_type::all_roles);
		}
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::change_indexes(int_vector::const_iterator first, int_vector::const_iterator last, int offset)
	{
		auto * model = get_model();
		auto size = last - first;

		auto list = model->persistentIndexList();
		for (const auto & idx : list)
		{
			if (!idx.isValid()) continue;

			auto row = idx.row();
			auto col = idx.column();

			if (row < offset) continue;

			assert(row < size); (void)size;
			auto newIdx = model->index(first[row - offset], col);
			model->changePersistentIndex(idx, newIdx);
		}
	}


	template <class Traits>
	void sfset_model_qtbase<Traits>::merge_newdata(
			value_ptr_iterator first, value_ptr_iterator middle, value_ptr_iterator last, bool resort_old /*= true*/)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_indirect_functor(std::cref(m_sort_pred));

		if (resort_old) varalgo::stable_sort(first, middle, comp);
		varalgo::sort(middle, last, comp);
		varalgo::inplace_merge(first, middle, last, comp);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::merge_newdata(
			value_ptr_iterator first, value_ptr_iterator middle, value_ptr_iterator last,
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

	template <class Traits>
	void sfset_model_qtbase<Traits>::stable_sort(value_ptr_iterator first, value_ptr_iterator last)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_indirect_functor(std::cref(m_sort_pred));
		varalgo::stable_sort(first, last, comp);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::stable_sort(
			value_ptr_iterator first, value_ptr_iterator last,
			int_vector_iterator ifirst, int_vector_iterator ilast)
	{
		if (not active(m_sort_pred)) return;

		auto comp = viewed::make_get_functor<0>(viewed::make_indirect_functor(std::cref(m_sort_pred)));

		auto zfirst = ext::make_zip_iterator(first, ifirst);
		auto zlast = ext::make_zip_iterator(last, ilast);
		varalgo::stable_sort(zfirst, zlast, comp);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::sort_and_notify()
	{
		if (not active(m_sort_pred)) return;

		auto & container = m_store;
		auto & seq_view  = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		constexpr int offset = 0;
		value_ptr_vector elements;
		int_vector indexes;

		elements.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		auto first = elements.begin();
		auto last  = elements.end();

		indexes.resize(elements.size());
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

		container.rearrange(boost::make_transform_iterator(first, make_ref));

		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->VerticalSortHint);
	}

	template <class Traits>
	auto sfset_model_qtbase<Traits>::search_hint(const_pointer ptr) const -> search_hint_type
	{
		if (not active(m_sort_pred)) return {m_store.begin(), m_store.end()};

		auto first = m_store.begin();
		auto last  = first + m_nvisible;

		return varalgo::equal_range(first, last, *ptr, m_sort_pred);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::refilter_and_notify(refilter_type rtype)
	{
		switch (rtype)
		{
			default:
			case refilter_type::same:        return;

			case refilter_type::incremental: return refilter_incremental_and_notify();
			case refilter_type::full:        return refilter_full_and_notify();
		}
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::refilter_incremental_and_notify()
	{
		if (not active(m_filter_pred)) return;

		auto * model = get_model();
		model->layoutAboutToBeChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);

		auto & container = m_store;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;
		constexpr int offset = 0;
		int nvisible = m_nvisible;

		// implementation is similar to refilter_full_and_notify,
		// but more simple - only visible area should filtered, and no sorting should be done
		// refilter_full_and_notify - for more description

		value_ptr_vector valptr_vector;
		int_vector index_array;

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		index_array.resize(seq_ptr_view.size());

		auto fpred  = viewed::make_indirect_functor(std::cref(m_filter_pred));
		auto zfpred = viewed::make_get_functor<0>(fpred);

		auto vfirst = valptr_vector.begin();
		auto vlast  = vfirst + nvisible;

		auto ivfirst = index_array.begin();
		auto ivlast  = ivfirst + nvisible;
		auto isfirst = ivlast;
		auto islast  = index_array.end();

		std::iota(ivfirst, islast, offset);

		auto[vpp, ivpp] = std::stable_partition(
			ext::make_zip_iterator(vfirst, ivfirst),
			ext::make_zip_iterator(vlast, ivlast),
			zfpred).get_iterator_tuple();

		std::transform(ivpp, ivlast, ivpp, viewed::mark_index);

		seq_view.rearrange(boost::make_transform_iterator(vfirst, make_ref));
		m_nvisible = vpp - vfirst;

		// recalculate qt persistent indexes and notify any clients
		viewed::inverse_index_array(index_array.begin(), index_array.end(), offset);
		change_indexes(index_array.begin(), index_array.end(), offset);

		model->layoutChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::refilter_full_and_notify()
	{
		if (not active(m_filter_pred) and m_nvisible == m_store.size()) return;

		auto * model = get_model();
		model->layoutAboutToBeChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);

		auto & container = m_store;
		auto & seq_view = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;
		constexpr int offset = 0;
		int nvisible_new, nvisible = m_nvisible;

		value_ptr_vector valptr_vector;
		int_vector index_array;


		// We must rearrange children according to sorting/filtering criteria.
		// Note when working with visible area - order of visible elements should not changed - we want stability.
		// Also Qt persistent indexes should be recalculated.
		//
		// Because children are stored in boost::multi_index_container we must copy pointer to elements into separate vector,
		// rearrange them, and than call boost::multi_index_container::rearrange
		//
		// layout of elements at start:
		//
		// |vfirst                     |vlast                      |valptr_vector.end()
		// ---------------------------------------------------------
		// |    visible elements       |      shadow elements      |
		// ---------------------------------------------------------
		// |valptr_vector.begin()      |sfirst                     |slast
		//

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		index_array.resize(seq_ptr_view.size());

		auto fpred  = viewed::make_indirect_functor(std::cref(m_filter_pred));
		auto zfpred = viewed::make_get_functor<0>(fpred);

		auto vfirst = valptr_vector.begin();
		auto vlast  = vfirst + nvisible;
		auto sfirst = vlast;
		auto slast  = valptr_vector.end();

		// elements index array - it will be permutated with elements array, later it will be used for recalculating qt indexes
		auto ivfirst = index_array.begin();
		auto ivlast  = ivfirst + nvisible;
		auto isfirst = ivlast;
		auto islast  = index_array.end();
		std::iota(ivfirst, islast, offset);

		if (not viewed::active(m_filter_pred))
		{
			// if there is now filter - just merge shadow area with visible
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
		seq_view.rearrange(boost::make_transform_iterator(vfirst, make_ref));
		m_nvisible = nvisible_new;

		// recalculate qt persistent indexes and notify any clients
		viewed::inverse_index_array(index_array.begin(), index_array.end(), offset);
		change_indexes(index_array.begin(), index_array.end(), offset);

		model->layoutChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);
	}

	template <class Traits>
	template <class ... Args>
	auto sfset_model_qtbase<Traits>::filter_by(Args && ... args) -> refilter_type
	{
		auto rtype = m_filter_pred.set_expr(std::forward<Args>(args)...);
		refilter_and_notify(rtype);

		return rtype;
	}

	template <class Traits>
	template <class ... Args>
	void sfset_model_qtbase<Traits>::sort_by(Args && ... args)
	{
		m_sort_pred = sort_pred_type(std::forward<Args>(args)...);
		sort_and_notify();
	}



	template <class Traits>
	void sfset_model_qtbase<Traits>::rearrange_and_notify(upsert_context & ctx)
	{
		auto * model = get_model();
		Q_EMIT model->layoutAboutToBeChanged(model_type::empty_model_list, model_type::NoLayoutChangeHint);

		auto & container = *ctx.container;
		auto & seq_view  = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		constexpr int offset = 0;
		auto nvisible = *ctx.nvisible_ptr;
		decltype(nvisible) nvisible_new;
		auto inserted_count = container.size() - ctx.size;

		value_ptr_vector & valptr_vector = *ctx.vptr_array;
		int_vector & index_array = *ctx.index_array;

		auto fpred  = viewed::make_indirect_functor(std::cref(m_filter_pred));

		// We must rearrange children according to sorting/filtering criteria.
		// * some elements have been erased - those must be erased
		// * some elements have been inserted - those should placed in visible or shadow area if they do not pass filtering
		// * some elements have changed - those can move from/to visible/shadow area
		// * when working with visible area - order of visible elements should not changed - we want stability
		// And then visible elements should be resorted according to sorting criteria.
		// And Qt persistent indexes should be recalculated.
		// Removal of elements from boost::multi_index_container is O(n) operation, where n is distance from position to end of sequence,
		// so we better move those at back, and than remove them all at once - that way removal will be O(1)
		//
		// Because children are stored in boost::multi_index_container we must copy pointer to elements into separate vector,
		// rearrange them, and than call boost::multi_index_container::rearrange
		//
		// layout of elements at start:
		//
		// |vfirst                     |vlast                      |nfirst              |nlast
		// ------------------------------------------------------------------------------
		// |    visible elements       |      shadow elements      |    new elements    |
		// ------------------------------------------------------------------------------
		// |valptr_vector.begin()      |sfirst                     |slast               |valptr_vector.end()
		//

		valptr_vector.assign(seq_ptr_view.begin(), seq_ptr_view.end());
		auto vfirst = valptr_vector.begin();
		auto vlast  = vfirst + nvisible;
		auto sfirst = vlast;
		auto slast  = vfirst + (seq_ptr_view.size() - inserted_count);
		auto nfirst = slast;
		auto nlast  = valptr_vector.end();

		// elements index array - it will be permutated with elements array, later it will be used for recalculating qt indexes
		index_array.resize(container.size());
		auto ifirst  = index_array.begin();
		auto imiddle = ifirst + nvisible;
		auto ilast   = index_array.end();
		std::iota(ifirst, ilast, offset); // at start: elements are placed in natural order: [0, 1, 2, ...]

		// [ctx.changed_first; ctx.changed_last) - indexes of changed elements
		// indexes < nvisible - are visible elements, others - are shadow.
		// partition that range by visible/shadow elements
		auto vchanged_first = ctx.changed_first;
		auto vchanged_last = std::partition(ctx.changed_first, ctx.changed_last,
			[nvisible](int index) { return index < nvisible; });

		// now [vchanged_first; vchanged_last) - indexes of changed visible elements
		//     [schanged_first; schanged_last) - indexes of changed shadow elements
		auto schanged_first = vchanged_last;
		auto schanged_last = ctx.changed_last;

		// partition visible indexes by those passing filtering predicate
		auto index_pass_pred = [vfirst, fpred](int index) { return fpred(vfirst[index]); };
		auto vchanged_pp = viewed::active(m_filter_pred)
			? std::partition(vchanged_first, vchanged_last, index_pass_pred)
			: vchanged_last;

		// now [vchanged_first; vchanged_pp) - indexes of visible elements passing filtering predicate
		//     [vchanged_pp; vchanged_last)  - indexes of visible elements not passing filtering predicate

		// mark removed ones by nullifying them, this will affect both shadow and visible elements
		// while we sort of forgetting about them, in fact we can always easily take them from seq_ptr_view, we still have their indexes
		for (auto it = ctx.removed_first; it != ctx.removed_last; ++it)
		{
			int index = *it;
			vfirst[index] = nullptr;
			ifirst[index] = -1;
		}

		// mark updated ones not passing filter predicate [vchanged_pp; vchanged_last) by nullifying them,
		// they have to be moved to shadow area, we will restore them little bit later
		for (auto it = vchanged_pp; it != vchanged_last; ++it)
		{
			int index = *it;
			vfirst[index] = nullptr;
			ifirst[index] = -1;
		}

		// current layout of elements:
		//  X - nullified
		//
		// |vfirst                     |vlast                      |nfirst               nlast
		// -------------------------------------------------------------------------------
		// | | | | | |X| | |X| | | | | | | | | | |X| | |X| | | | | | | | | | | | | | | | |
		// -------------------------------------------------------------------------------
		// |                           |sfirst                     |slast

		if (not viewed::active(m_filter_pred))
		{
			// remove marked elements from [vfirst; vlast)
			vlast  = std::remove(vfirst, vlast, nullptr);
			// remove marked elements from [sfirst; slast) but in reverse direction, gathering them near new elements
			sfirst = std::remove(std::make_reverse_iterator(slast), std::make_reverse_iterator(sfirst), nullptr).base();
			// and now just move gathered next to [vfirst; vlast)
			nlast  = std::move(sfirst, nlast, vlast);
			nvisible_new = nlast - vfirst;
		}
		else
		{
			// mark elements from shadow area passing filter predicate, by toggling lowest pointer bit
			for (auto it = schanged_first; it != schanged_last; ++it)
				if (index_pass_pred(*it)) vfirst[*it] = viewed::mark_pointer(vfirst[*it]);

			// current layout of elements:
			//  X - nullified, M - marked via lowest pointer bit
			//
			// |vfirst                     |vlast                      |nfirst               nlast
			// -------------------------------------------------------------------------------
			// | | | | | |X| | |X| | | | | | | | |M|M|X| | |X| | |M| | | | | | | | | | | | | |
			// -------------------------------------------------------------------------------
			// |                           |sfirst                     |slast

			// now remove X elements from [vfirst; vlast) - those are removed elements and should be completely removed
			vlast  = std::remove(vfirst, vlast, nullptr);
			// and remove X elements from [sfirst; slast) but in reverse direction, gathering them near new elements
			sfirst = std::remove(std::make_reverse_iterator(slast), std::make_reverse_iterator(sfirst), nullptr).base();

			// current layout of elements:
			//  X - nullified, M - marked via lowest pointer bit
			//
			//                             vlast <- 2 elements
			// |vfirst                 |vlast                          |nfirst               nlast
			// -------------------------------------------------------------------------------
			// | | | | | | | | | | | | | | | | | | | |M|M| | | | |M| | | | | | | | | | | | | |
			// -------------------------------------------------------------------------------
			// |                               |sfirst                 |slast
			//                             sfirst -> 2 elements

			// [spp, npp) - gathered elements from [sfirst, nlast) satisfying fpred
			auto spp = std::partition(sfirst, slast, [](auto * ptr) { return not viewed::marked_pointer(ptr); });
			auto npp = std::partition(nfirst, nlast, fpred);
			nvisible_new = (vlast - vfirst) + (npp - spp);

			// current layout of elements:
			//  X - nullified, M - marked via lowest pointer bit, S - not marked via lowest bit(not passing filter predicate)
			//  P - new elements passing filter predicate, N - new elements not passing filter predicate
			//
			// |vfirst                 |vlast                          |nfirst               nlast
			// -------------------------------------------------------------------------------
			// | | | | | | | | | | | | | | | | |S|S|S|S|S|S|S|S|S|M|M|M|P|P|P|P|P|P|N|N|N|N|N|
			// -------------------------------------------------------------------------------
			// |                               |sfirst           |     |slast      |
			//                                                   |spp              |npp

			// unmark any marked pointers
			for (auto it = spp; it != slast; ++it)
				*it = unmark_pointer(*it);

			// rotate [spp, npp) at the beginning of shadow area
			// and in fact merge those with visible area
			std::rotate(sfirst, spp, npp);
			nlast = std::move(sfirst, nlast, vlast);
		}

		// at that point we removed erased elements, but we need to rearrange boost::multi_index_container via rearrange method
		// and it expects all elements that it has - we need to add removed ones at the end of rearranged array - and them we will remove them from container.
		// [rfirst; rlast) - elements to be removed.
		auto rfirst = nlast;
		auto rlast = rfirst;

		{   // remove nullified elements from index array
			auto point = imiddle;
			imiddle = std::remove(ifirst, imiddle, -1);
			ilast = std::remove(point, ilast, -1);
			ilast = std::move(point, ilast, imiddle);
		}

		// restore elements from [vchanged_pp; vchanged_last), add them at the end of shadow area
		for (auto it = vchanged_pp; it != vchanged_last; ++it)
		{
			int index = *it;
			*rlast++ = seq_ptr_view[index];
			*ilast++ = viewed::mark_index(index);
		}

		// temporary restore elements from [removed_first; removed_last), after container rearrange - they will be removed
		for (auto it = ctx.removed_first; it != ctx.removed_last; ++it)
		{
			int index = *it;
			*rlast++ = seq_ptr_view[index];
			*ilast++ = viewed::mark_index(index);
		}

		// if some elements from visible area are changed and they still in the visible area - we need to resort them => resort whole visible area.
		bool resort_old = vchanged_first != vchanged_pp;
		// resort visible area, merge new elements and changed from shadow area(std::stable sort + std::inplace_merge)
		merge_newdata(vfirst, vlast, nlast, ifirst, imiddle, ifirst + (nlast - vfirst), resort_old);

		// at last, rearranging is over -> set order it boost::multi_index_container
		constexpr auto make_ref = [](auto * ptr) { return std::ref(*ptr); };
		seq_view.rearrange(boost::make_transform_iterator(vfirst, make_ref));

		// and erase removed elements
		seq_view.resize(seq_view.size() - (ctx.removed_last - ctx.removed_first));
		*ctx.nvisible_ptr = nvisible_new;

		// recalculate qt persistent indexes and notify any clients
		viewed::inverse_index_array(ifirst, ilast, offset);
		change_indexes(index_array.begin(), index_array.end(), offset);

		Q_EMIT model->layoutChanged(model_type::empty_model_list, model->VerticalSortHint);
	}


	template <class Traits>
	template <class SinglePassIterator>
	void sfset_model_qtbase<Traits>::assign(SinglePassIterator first, SinglePassIterator last)
	{
		auto & container = m_store;
		auto & code_view = container.template get<by_code>();
		auto & seq_view  = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		value_ptr_vector existing_elements;
		int_vector index_array, affected_indexes;
		affected_indexes.resize(m_store.size());
		existing_elements.assign(seq_ptr_view.begin(), seq_ptr_view.end());

		upsert_context ctx;
		ctx.size         = seq_view.size();
		ctx.nvisible_ptr = &m_nvisible;
		ctx.container    = &m_store;
		ctx.index_array  = &index_array;
		ctx.vptr_array   = &existing_elements;

		auto & removed_first = ctx.removed_first = affected_indexes.end();
		auto & removed_last  = ctx.removed_last  = affected_indexes.end();

		auto & changed_first = ctx.changed_first = affected_indexes.begin();
		auto & changed_last  = ctx.changed_last  = affected_indexes.begin();


		for (; first != last; ++first)
		{
			auto && val = *first;

			typename code_view_type::const_iterator where;
			bool inserted_into_store;
			std::tie(where, inserted_into_store) = code_view.insert(std::forward<decltype(val)>(val));

			if (not inserted_into_store)
			{
				traits_type::update(ext::unconst(*where), std::forward<decltype(val)>(val));
				auto pos = m_store.template project<by_seq>(where) - seq_view.begin();
				auto & ptr = existing_elements[pos];
				ptr = viewed::mark_pointer(ptr);
			}
		}

		auto elements_first = existing_elements.begin();
		auto elements_last  = existing_elements.end();

		int pos = 0;
		for (auto it = elements_first; it != elements_last; ++it, ++pos)
		{
			if (viewed::marked_pointer(*it))
				*changed_last++ = pos;
			else
				*--removed_first = pos;
		}

		rearrange_and_notify(ctx);
	}

	template <class Traits>
	template <class SinglePassIterator>
	void sfset_model_qtbase<Traits>::upsert(SinglePassIterator first, SinglePassIterator last)
	{
		auto & container = m_store;
		auto & code_view = container.template get<by_code>();
		auto & seq_view  = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		value_ptr_vector vptr_array;
		int_vector index_array, affected_indexes;
		affected_indexes.resize(m_store.size());
		vptr_array.reserve(container.size());

		upsert_context ctx;
		ctx.size         = seq_view.size();
		ctx.nvisible_ptr = &m_nvisible;
		ctx.container    = &container;
		ctx.index_array  = &index_array;
		ctx.vptr_array   = &vptr_array;

		auto & removed_first = ctx.removed_first = affected_indexes.end();
		auto & removed_last  = ctx.removed_last  = affected_indexes.end();

		auto & changed_first = ctx.changed_first = affected_indexes.begin();
		auto & changed_last  = ctx.changed_last  = affected_indexes.begin();


		for (; first != last; ++first)
		{
			auto && val = *first;

			typename code_view_type::const_iterator where;
			bool inserted_into_store;
			std::tie(where, inserted_into_store) = code_view.insert(std::forward<decltype(val)>(val));

			if (not inserted_into_store)
			{
				traits_type::update(ext::unconst(*where), std::forward<decltype(val)>(val));
				auto pos = container.template project<by_seq>(where) - seq_view.begin();
				*changed_last++ = pos;
			}
		}

		rearrange_and_notify(ctx);
	}

	template <class Traits>
	void sfset_model_qtbase<Traits>::clear()
	{
		auto * model = get_model();
		model->beginResetModel();
		m_store.clear();
		m_nvisible = 0;
		model->endResetModel();
	}

	template <class Traits>
	auto sfset_model_qtbase<Traits>::erase(const_iterator first, const_iterator last) -> const_iterator
	{
		if (first == last) return first;

		auto & container = m_store;
		auto & seq_view = container.template get<by_seq>();

		assert(first <= last);
		assert(seq_view.begin() <= first and last <= seq_view.end());

		int first_pos = first - seq_view.begin();
		int last_pos  = last  - seq_view.begin();

		auto * model = get_model();
		model->beginRemoveRows(model_type::invalid_index, first_pos, last_pos - 1);
		auto ret = container.erase(first, last);
		m_nvisible -= std::max<int>(0, m_nvisible - std::min<size_type>(last_pos, m_nvisible));
		model->endRemoveRows();

		return ret;
	}

	template <class Traits>
	template <class CompatibleKey>
	auto sfset_model_qtbase<Traits>::erase(const CompatibleKey & key) -> size_type
	{		
		auto & container = m_store;
		auto & code_view = container.template get<by_code>();
		auto & seq_view  = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		auto it = code_view.find(key);
		if (it == code_view.end()) return 0;

		int pos = container.template project<by_seq>(it) - seq_view.begin();

		auto * model = get_model();
		model->beginRemoveRows(model_type::invalid_index, pos, pos);
		code_view.erase(it);
		m_nvisible -= 1;
		model->endRemoveRows();

		return 1;
	}

	template <class Traits>
	template <class SinglePassIterator>
	auto sfset_model_qtbase<Traits>::erase(SinglePassIterator first, SinglePassIterator last) ->
		std::enable_if_t<std::is_convertible_v<ext::iterator_value_t<SinglePassIterator>, key_type>, size_type>
	{
		if (first == last) return 0;

		auto & container = m_store;
		auto & code_view = container.template get<by_code>();
		auto & seq_view  = container.template get<by_seq>();
		auto seq_ptr_view = seq_view | ext::outdirected;

		value_ptr_vector vptr_array;
		int_vector index_array, affected_indexes;
		affected_indexes.resize(m_store.size());
		vptr_array.reserve(container.size());

		upsert_context ctx;
		ctx.size         = seq_view.size();
		ctx.nvisible_ptr = &m_nvisible;
		ctx.container    = &container;
		ctx.index_array  = &index_array;
		ctx.vptr_array   = &vptr_array;

		auto & removed_first = ctx.removed_first = affected_indexes.end();
		auto & removed_last  = ctx.removed_last  = affected_indexes.end();

		auto & changed_first = ctx.changed_first = affected_indexes.begin();
		auto & changed_last  = ctx.changed_last  = affected_indexes.begin();


		for (; first != last; ++first)
		{
			auto && key = *first;

			auto it = code_view.find(key);
			if (it == code_view.end()) continue;

			int pos = container.template project<by_seq>(it) - seq_view.begin();
			*--removed_first = pos;
		 }

		auto removed = removed_last - removed_first;
		rearrange_and_notify(ctx);
		return removed;
	}

}


#if BOOST_COMP_GNUC
#pragma GCC diagnostic pop
#endif
