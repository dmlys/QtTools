#pragma once
#include <cstddef>
#include <cstdint>
#include <climits> // for CHAR_BTI
#include <type_traits>
#include <vector>
#include <algorithm>
#include <numeric>
#include <ext/type_traits.hpp>

#include <varalgo/std_variant_traits.hpp>
#include <viewed/qt_model.hpp>

namespace viewed
{
	class AbstractItemModel;

	namespace detail
	{
		template <class Pred> static std::enable_if_t<    ext::static_castable_v<Pred, bool>, bool> active_visitor(const Pred & pred) { return static_cast<bool>(pred); }
		template <class Pred> static std::enable_if_t<not ext::static_castable_v<Pred, bool>, bool> active_visitor(const Pred & pred) { return true; }
	}

	template <class Pred> static bool active(Pred && pred)
	{
		auto vis = [](const auto & pred) { return detail::active_visitor(pred); };
		return varalgo::variant_traits<std::decay_t<Pred>>::visit(vis, std::forward<Pred>(pred));
	}

	namespace detail
	{
		constexpr int INDEX_MARK_MASK = 1 << (sizeof(int) * CHAR_BIT - 1);
		constexpr int INDEX_UNMARK_MASK = ~INDEX_MARK_MASK;
		static_assert(INDEX_MARK_MASK == INT_MIN, "some very unusual architecture here");
	}

	struct mark_pointer_type
	{
		template <class Type>
		constexpr Type operator()(Type ptr) const noexcept
		{
			static_assert(std::is_pointer_v<Type>);
			auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr) | 1;
			return reinterpret_cast<Type>(ptr_val);
		}
	};

	struct unmark_pointer_type
	{
		template <class Type>
		constexpr Type operator()(Type ptr) const noexcept
		{
			static_assert(std::is_pointer_v<Type>);
			auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr) & ~static_cast<std::uintptr_t>(1);
			return reinterpret_cast<Type>(ptr_val);
		}
	};

	struct toggle_pointer_mark_type
	{
		template <class Type>
		constexpr Type operator()(Type ptr) const noexcept
		{
			static_assert(std::is_pointer_v<Type>);
			auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr) ^ static_cast<std::uintptr_t>(1);
			return reinterpret_cast<Type>(ptr_val);
		}
	};

	struct marked_pointer_type
	{
		template <class Type>
		constexpr bool operator()(Type ptr) const noexcept
		{
			static_assert(std::is_pointer_v<Type>);
			return reinterpret_cast<std::uintptr_t>(ptr) & 1;
		}
	};

	struct mark_index_type
	{
		constexpr int operator()(int idx) const noexcept
		{
			return idx | detail::INDEX_MARK_MASK;
		}
	};

	struct unmark_index_type
	{
		constexpr int operator()(int idx) const noexcept
		{
			return idx & detail::INDEX_UNMARK_MASK;
		}
	};

	struct toggle_index_mark_type
	{
		constexpr int operator()(int idx) const noexcept
		{
			return idx ^ detail::INDEX_MARK_MASK;
		}
	};

	struct marked_index_type
	{
		constexpr bool operator()(int idx) const noexcept
		{
			return idx & detail::INDEX_MARK_MASK;
		}
	};


	constexpr mark_pointer_type   mark_pointer {};
	constexpr unmark_pointer_type unmark_pointer {};
	constexpr marked_pointer_type marked_pointer {};
	constexpr toggle_pointer_mark_type toggle_pointer_mark {};

	constexpr mark_index_type   mark_index {};
	constexpr unmark_index_type unmark_index {};
	constexpr marked_index_type marked_index {};
	constexpr toggle_index_mark_type toggle_index_mark {};


	/// inverses index array in following way:
	/// inverse[arr[i] - offset] = i for first..last.
	/// This is for when you have array of arr[new_index] => old_index,
	/// but need arr[old_index] => new_idx for qt changePersistentIndex
	template <class RandomAccessIterator>
	void inverse_index_array(RandomAccessIterator first, RandomAccessIterator last,
	                         typename std::iterator_traits<RandomAccessIterator>::value_type offset = 0)
	{
		typedef typename std::iterator_traits<RandomAccessIterator>::value_type value_type;
		static_assert(std::is_signed<value_type>::value, "index type must be signed");
		
		std::vector<value_type> inverse(last - first);
		auto i = static_cast<value_type>(offset);

		for (auto it = first; it != last; ++it, ++i)
		{
			value_type val = *it;
			inverse[unmark_index(val) - offset] = not marked_index(val) ? i : -1;
		}

		std::copy(inverse.begin(), inverse.end(), first);
	}
	
	/// relloc map describes where elements were moved while removing elements.
	/// it's index array, where index itself - old index, and element - new_index: arr[old_index] => new_index,
	/// it is what view_qtbase::change_indexes expects with offset 0
	/// 
	/// [removed_first; removed_last) index range of removed elements, for example:
	/// [0, 5, 7] elements by indexes 0, 5, 7 were removed as if by std::remove_if algorithm.
	template <class Iterator>
	auto build_relloc_map(Iterator removed_first, Iterator removed_last, std::size_t store_size)
		-> std::vector<typename std::iterator_traits<Iterator>::value_type>
	{
		typedef typename std::iterator_traits<Iterator>::value_type value_type;
		static_assert(std::is_signed<value_type>::value, "index type must be signed");
		
		std::vector<value_type> index_array(store_size);
		value_type v1 = 0, v2 = static_cast<value_type>(store_size);
		value_type val = 0;

		for (; removed_first != removed_last; ++removed_first)
		{
			v2 = *removed_first;

			auto first = index_array.begin() + v1;
			auto last = index_array.begin() + v2;
			std::iota(first, last, val);
			val += static_cast<value_type>(last - first);

			*last = -1;
			v1 = ++v2;
		}

		auto first = index_array.begin() + v2;
		auto last = index_array.end();
		std::iota(first, last, val);

		return index_array;
	}

	/// removes elements from [first, last) by indexes given in [ifirst, ilast)
	template <class RandomAccessIterator, class Iterator>
	RandomAccessIterator remove_indexes(RandomAccessIterator first, RandomAccessIterator last,
	                                    Iterator ifirst, Iterator ilast)
	{
		if (ifirst == ilast) return last;

		auto out = first + *ifirst;
		auto it = out + 1;
		auto next = last;

		for (++ifirst; ifirst != ilast; ++ifirst)
		{
			next = first + *ifirst;
			out = std::move(it, next, out);
			it = next + 1;
		}

		return std::move(it, last, out);
	}


	/// emits qt signal model->dataChanged about changed rows. Changed rows are defined by indexes stored in [first; last)
	template <class SinglePassIterator>
	void emit_changed(AbstractItemModel * model, SinglePassIterator first, SinglePassIterator last)
	{
		if (first == last) return;

		int ncols = model->columnCount(AbstractItemModel::invalid_index);

		for (; first != last; ++first)
		{
			// lower index on top, higher on bottom
			int top, bottom;
			top = bottom = *first;

			// try to find the sequences with step of 1, for example: ..., 4, 5, 6, ...
			for (++first; first != last and *first - bottom == 1; ++first, ++bottom)
				continue;

			--first;

			auto top_left = model->index(top, 0, AbstractItemModel::invalid_index);
			auto bottom_right = model->index(bottom, ncols - 1, AbstractItemModel::invalid_index);
			Q_EMIT model->dataChanged(top_left, bottom_right, AbstractItemModel::all_roles);
		}
	}

	/// changes persistent indexes via get_model->changePersistentIndex.
	/// [first; last) - range where range[oldIdx - offset] => newIdx.
	/// if newIdx < 0 - index should be removed(changed on invalid, qt supports it)
	template <class SinglePassIterator>
	void change_indexes(AbstractItemModel * model, SinglePassIterator first, SinglePassIterator last, int offset)
	{
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
}
