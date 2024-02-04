#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QLayout>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMdiSubWindow>

#include <QtTools/ToolsBase.hpp>
#include <QtTools/ItemViewUtils.hpp>


namespace QtTools
{
	class TreeViewHack : public QTreeView
	{
	public:
		using QTreeView::rowHeight;
	};

	int CalculateDefaultRowHeight(const QTableView * view)
	{
		return 21;
	}

	int HeaderWidth(const QHeaderView * header)
	{
		int count = header->count();
		int width = 0;
		for (int i = 0; i < count; ++i)
			width += header->sectionSize(i);

		return width;
	}

	int ItemViewWidthHint(const QListView * view, bool withScrollBar)
	{
		const auto * model = view->model();
		if (not model) return view->sizeHint().width();
		
		constexpr QModelIndex parent;
		const int rc = model->rowCount(parent);
		const int mc = view->modelColumn();
		
		int width = 0;
		QRect area;
		
		for (int i = 0; i < rc; ++i)
		{
			auto rect = view->visualRect(model->index(i, mc, parent));
			area |= rect;
			width = area.width();
		}

		width += view->frameWidth() * 2;
		
		if (withScrollBar)
		{
			auto * style = view->style();
			width += style->pixelMetric(QStyle::PM_ScrollBarExtent);
		}
		
		return width;
	}
	
	int ItemViewHeightHint(const QListView * view, bool withScrollBar)
	{
		const auto * model = view->model();
		if (not model) return view->sizeHint().width();
		
		constexpr QModelIndex parent;
		const int rc = model->rowCount(parent);
		const int mc = view->modelColumn();
		
		int height = 0;
		QRect area;
		
		for (int i = 0; i < rc; ++i)
		{
			auto rect = view->visualRect(model->index(i, mc, parent));
			area |= rect;
			height = area.height();
		}

		height += view->frameWidth() * 2;
		
		if (withScrollBar)
		{
			auto * style = view->style();
			height += style->pixelMetric(QStyle::PM_ScrollBarExtent);
		}
		
		return height;
	}
	
	int ItemViewWidthHint(const QTableView * view, bool withScrollBar)
	{
		// смотри также реализацию ItemViewSizeHint, там есть важные пояснения
		auto * model = view->model();
		if (not model) return view->sizeHint().width();

		int count = model->columnCount();
		int width = 0;
		
		for (int i = 0; i < count; ++i)
			width += view->columnWidth(i);

		width += view->frameWidth() * 2;
		auto * vhdr = view->verticalHeader();
		if (not vhdr->isHidden())
			width += vhdr->width();

		if (withScrollBar)
		{
			auto * style = view->style();
			width += style->pixelMetric(QStyle::PM_ScrollBarExtent);
		}

		return width;
	}

	int ItemViewHeightHint(const QTableView * view, bool withScrollBar)
	{
		// смотри также реализацию ItemViewSizeHint, там есть важные пояснения
		auto * model = view->model();
		if (not model) return view->sizeHint().height();

		int count = model->rowCount();
		int height = 0;

		for (int i = 0; i < count; ++i)
			height += view->rowHeight(i);

		height += view->frameWidth() * 2;
		auto * hhdr = view->horizontalHeader();
		if (not hhdr->isHidden())
			height += hhdr->height();

		if (withScrollBar)
		{
			auto * style = view->style();
			height += style->pixelMetric(QStyle::PM_ScrollBarExtent);
		}

		return height;
	}

	int ItemViewWidthHint(const QTreeView * view, bool withScrollBar)
	{
		// смотри также реализацию ItemViewSizeHint, там есть важные пояснения
		auto * model = view->model();
		if (not model) return view->sizeHint().width();

		int count = model->columnCount();
		int width = 0;

		for (int i = 0; i < count; ++i)
			width += view->columnWidth(i);

		width += view->frameWidth() * 2;

		if (withScrollBar)
		{
			auto * style = view->style();
			width += style->pixelMetric(QStyle::PM_ScrollBarExtent);
		}

		return width;
	}

	int ItemViewHeightHint(const QTreeView * view, bool withScrollBar)
	{
		// смотри также реализацию TableSizeHint, там есть важные пояснения
		auto * model = view->model();
		if (not model) return view->sizeHint().width();

		QRect visualRect;

		for (auto index = view->indexAt({0, 0}); index.isValid(); index = view->indexBelow(index))
			visualRect |= view->visualRect(index);

		int height = visualRect.height() + view->frameWidth() * 2;
		auto * vhdr = view->header();
		if (not vhdr->isHidden())
			height += vhdr->width();

		if (withScrollBar)
		{
			auto * style = view->style();
			height += style->pixelMetric(QStyle::PM_ScrollBarExtent);
		}

		return height;
	}

	bool ScrollBarVisible(Qt::ScrollBarPolicy policy, int size, int maxSize)
	{
		switch (policy)
		{
			case Qt::ScrollBarAsNeeded:
				return size > maxSize;
			case Qt::ScrollBarAlwaysOff:
				return false;
			case Qt::ScrollBarAlwaysOn:
				return true;
			default:
				return false;
		}
	}

	QSize ItemViewSizeHint(const QListView * view, const QSize & minimum, const QSize & maximum, bool forceSB /* = false */)
	{
		const auto * model = view->model();
		if (not model) return view->sizeHint();
		
		constexpr QModelIndex parent;
		const int rc = model->rowCount(parent);
		const int mc = view->modelColumn();
		//const int cc = model->columnCount();
	
		int width = 0, height = 0;
		const int minWidth = minimum.width();
		const int minHeight = minimum.height();
		const int maxWidth = std::max(minWidth, maximum.width());
		const int maxHeight = std::max(minHeight, maximum.height());
	
		/// доп размеры
		const int frameWidth = view->frameWidth() * 2;
		const auto viewmode  = view->viewMode();
		if (viewmode != view->ListMode)
			return view->sizeHint();
		
		QRect area;
		for (int i = 0; i < rc && height < maxHeight && width < maxWidth; ++i)
		{
			auto rect = view->visualRect(model->index(i, mc, parent));
			area |= rect;
			width = area.width();
			height = area.height();
		}
	
		bool horizontalScrollBarVisisble = forceSB ||
			ScrollBarVisible(view->horizontalScrollBarPolicy(), width, maxWidth - frameWidth);
		bool vertiacalScroolBarVisible = forceSB ||
			ScrollBarVisible(view->verticalScrollBarPolicy(), height, maxHeight - frameWidth);
	
		const auto * style = view->style();
		const auto sbextent = style->pixelMetric(QStyle::PM_ScrollBarExtent);
	
		if (horizontalScrollBarVisisble)
			height += sbextent;
	
		if (vertiacalScroolBarVisible)
			width += sbextent;
	
		width += frameWidth;
		height += frameWidth;
	
		return {
			qBound(minWidth, width, maxWidth),
			qBound(minHeight, height, maxHeight)
		};
	}
	
	QSize ItemViewSizeHint(const QTableView * view, const QSize & minimum, const QSize & maximum, bool forceSB /* = false */)
	{
		const auto * model = view->model();
		if (not model) return view->sizeHint();
		
		const int rc = model->rowCount();
		const int cc = model->columnCount();

		int width = 0, height = 0;
		const int minWidth = minimum.width();
		const int minHeight = minimum.height();
		const int maxWidth = std::max(minWidth, maximum.width());
		const int maxHeight = std::max(minHeight, maximum.height());

		auto * hhdr = view->horizontalHeader();
		auto * vhdr = view->verticalHeader();

		/// доп размеры
		const int frameWidth = view->frameWidth() * 2;
		
		/// isVisible возвращает фактическую видимость, как результат совокупности всех факторов(видимость родителя, прочее)
		/// isHidden всегда возвращает явную скрытость данного виджета.
		/// Именно, виджет может быть одновременно не скрытым и не видимым.
		/// на момент вызова данной функции, например, из sizeHint(),
		/// мы можем находится в промежуточном состоянии изменения видимости иерархии виджетов,
		/// мы должны использовать isHidden.
		const int hhdrHeight = not hhdr->isHidden() ? hhdr->sizeHint().height() : 0;
		const int vhdrWidth = not vhdr->isHidden() ? vhdr->sizeHint().width() : 0;

		for (int i = 0; i < cc && width < maxWidth; ++i)
			width += view->columnWidth(i);

		for (int i = 0; i < rc && height < maxHeight; ++i)
			height += view->rowHeight(i);

		// columnWidth похоже учитывает grid size.
		/*
		// похоже что на текущий момент (Qt 5.4) gridSize для QTableView всегда 1
		// смотри: qtableview.cpp:1361
		// const int gridSize = view->showGrid() ? 1 : 0;
		// width += count * gridSize;
		*/

		bool horizontalScrollBarVisisble = forceSB ||
			ScrollBarVisible(view->horizontalScrollBarPolicy(), width, maxWidth - frameWidth - vhdrWidth);
		bool vertiacalScroolBarVisible = forceSB ||
			ScrollBarVisible(view->verticalScrollBarPolicy(), height, maxHeight - frameWidth - hhdrHeight);

		const auto * style = view->style();
		const auto sbextent = style->pixelMetric(QStyle::PM_ScrollBarExtent);

		if (horizontalScrollBarVisisble)
			height += sbextent;

		if (vertiacalScroolBarVisible)
			width += sbextent;

		width += frameWidth + vhdrWidth;
		height += frameWidth + hhdrHeight;

		return {
			qBound(minWidth, width, maxWidth),
			qBound(minHeight, height, maxHeight)
		};

	}

	QSize ItemViewSizeHint(const QTreeView * view, const QSize & minimum, const QSize & maximum, bool forceSB /* = false */)
	{
		const auto * model = view->model();
		if (not model) return view->sizeHint();

		//const int rc = model->rowCount();
		const int cc = model->columnCount();
		const int indent = view->indentation();

		int width = 0, height = 0;
		const int minWidth = minimum.width();
		const int minHeight = minimum.height();
		const int maxWidth = std::max(minWidth, maximum.width());
		const int maxHeight = std::max(minHeight, maximum.height());

		auto * hhdr = view->header();

		/// доп размеры
		const int frameWidth = view->frameWidth() * 2;

		/// isVisible возвращает фактическую видимость, как результат совокупности всех факторов(видимость родителя, прочее)
		/// isHidden всегда возвращает явную скрытость данного виджета.
		/// Именно, виджет может быть одновременно не скрытым и не видимым.
		/// на момент вызова данной функции, например, из sizeHint(),
		/// мы можем находится в промежуточном состоянии изменения видимости иерархии виджетов,
		/// мы должны использовать isHidden.
		const int hhdrHeight = not hhdr->isHidden() ? hhdr->sizeHint().height() : 0;
		QRect visualRect;

		for (int i = 0; i < cc && width < maxWidth; ++i)
			width += view->columnWidth(i);

		for (auto index = view->indexAt({0, 0}); index.isValid() and visualRect.width() < maxWidth; index = view->indexBelow(index))
			visualRect |= view->visualRect(index);

		height = visualRect.height();
		width = std::max(width, visualRect.width()) + indent;

		bool horizontalScrollBarVisisble = forceSB ||
			ScrollBarVisible(view->horizontalScrollBarPolicy(), width, maxWidth - frameWidth - 0);
		bool vertiacalScroolBarVisible = forceSB ||
			ScrollBarVisible(view->verticalScrollBarPolicy(), height, maxHeight - frameWidth - hhdrHeight);

		const auto * style = view->style();
		const auto sbextent = style->pixelMetric(QStyle::PM_ScrollBarExtent);

		if (horizontalScrollBarVisisble)
			height += sbextent;

		if (vertiacalScroolBarVisible)
			width += sbextent;

		width += frameWidth + 0;
		height += frameWidth + hhdrHeight;

		return {
			qBound(minWidth, width, maxWidth),
			qBound(minHeight, height, maxHeight)
		};
	}

	template <class ViewWidget>
	static QSize ItemViewSizeHint(const QWidget * us, const ViewWidget * view)
	{
		// maximum size - half screen
		QSize maxSize = QApplication::desktop()->screenGeometry(view).size();
		maxSize /= 2;
		// but no more than maximumSize()
		maxSize = maxSize.boundedTo(us->maximumSize());

		// if we are in QMdiArea, then our sizeHint should not be more than one third of QMdiArea size
		if (auto mdi = QtTools::FindAncestor<QMdiArea>(us))
		{
			maxSize = mdi->size();
			maxSize /= 3;
		}

		// additional size - size of all layout'а
		// minus size of tableView, size of which we calculate ourself.
		// QTableView::sizeHint in fact always return dummy size: 256:192
		QSize addSz = us->QWidget::sizeHint() - view->sizeHint();
		maxSize -= addSz;

		auto sz = QtTools::ItemViewSizeHint(view, view->minimumSizeHint(), maxSize);
		return sz += addSz;
	}

	QSize ItemViewSizeHint(const QWidget * us, const QListView * view)
	{
		return ItemViewSizeHint<QListView>(us, view);
	}
	
	QSize ItemViewSizeHint(const QWidget * us, const QTableView * view)
	{
		return ItemViewSizeHint<QTableView>(us, view);
	}

	QSize ItemViewSizeHint(const QWidget * us, const QTreeView * view)
	{
		return ItemViewSizeHint<QTreeView>(us, view);
	}

	QSize LayoutAdditionalSize(const QLayout * layout)
	{
		auto m = layout->contentsMargins();
		return {
			m.left() + m.right(),
			m.top() + m.bottom()
		};
	}


	void ResizeColumnsToContents(QTreeView * treeView)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		// what we need is done by resizeColumnsToContents,
		// but it's also takes into account headers, and i want without them - have to do by hand
		auto * header = treeView->header();
		auto minimum = header->minimumSectionSize();
		int count = header->count();

		for (int i = 0; i < count; ++i)
		{
			auto li = header->logicalIndex(i);
			if (not header->isSectionHidden(li))
			{
				// for some unknown reason virtual method sizeHintForColumn is declared as protected in QTableView,
				// through it's public QAbstractItemView. call through base class
				auto hint = static_cast<QAbstractItemView *>(treeView)->sizeHintForColumn(li);
				//auto headerHint = header->sectionSizeHint(li);
				header->resizeSection(i, std::max(hint, minimum));
			}
		}

		QApplication::restoreOverrideCursor();
	}

	void ResizeColumnsToContents(QTableView * tableView)
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		// what we need is done by resizeColumnsToContents,
		// but it's also takes into account headers, and i want without them - have to do by hand
		auto * header = tableView->horizontalHeader();
		auto minimum = header->minimumSectionSize();
		int count = header->count();

		for (int i = 0; i < count; ++i)
		{
			auto li = header->logicalIndex(i);
			if (not header->isSectionHidden(i))
			{
				// for some unknown reason virtual method sizeHintForColumn is declared as protected in QTableView,
				// through it's public QAbstractItemView. call through base class
				auto hint = static_cast<QAbstractItemView *>(tableView)->sizeHintForColumn(li);
				header->resizeSection(li, std::max(hint, minimum));
			}
		}

		QApplication::restoreOverrideCursor();
	}
}
