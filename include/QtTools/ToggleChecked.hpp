#pragma once
#include <iterator>
#include <QtCore/QModelIndex>
#include <QtCore/QAbstractItemModel>

namespace QtTools
{
	/// подсчитывает кол-во элементов из набора индексов в заданном состоянии
	template <class IndexList>
	int CheckedCount(const IndexList & indxes, Qt::CheckState state = Qt::Checked)
	{
		int checkedCount = 0;
		for (const auto & idx : indxes)
		{
			auto st = static_cast<Qt::CheckState>(idx.data(Qt::CheckStateRole).toInt());
			if (st == state) ++checkedCount;
		}

		return checkedCount;
	}

	/// Меняет состояние элементов из набора индексов indxes.
	///   Если все элементы checked или unckecked - переводит все в противоположенное состояние
	///   приводит все элементы к большей группе
	///   
	/// @Param indxes набор элементов
	/// @Param checkedCount кол-во выбранных элементов
	template <class IndexList>
	void ToggleChecked(IndexList & indxes, int checkedCount)
	{
		auto sz = indxes.size();
		bool checked = checkedCount == 0 || (checkedCount != sz && checkedCount >= sz / 2);

		typedef typename IndexList::value_type value_type;
		for (value_type & idx : indxes)
		{
			auto * model = const_cast<QAbstractItemModel *>(idx.model());
			model->setData(idx, checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
		}
	}

	/// Меняет состояние элементов из набора индексов indxes.
	///   Если все элементы checked или unckecked - переводит все в противоположенное состояние
	///   приводит все элементы к большей группе
	///
	/// @Param indxes набор элементов
	template <class IndexList>
	inline void ToggleChecked(IndexList & indxes)
	{
		ToggleChecked(indxes, CheckedCount(indxes));
	}
}
