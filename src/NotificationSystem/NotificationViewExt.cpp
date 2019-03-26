#include <QtTools/NotificationSystem/NotificationSystem.hqt>
#include <QtTools/NotificationSystem/NotificationSystemExt.hqt>
#include <QtTools/NotificationSystem/NotificationView.hqt>
#include <QtTools/NotificationSystem/NotificationViewExt.hqt>

namespace QtTools::NotificationSystem
{
	viewed::refilter_type NotificationFilter::set_expr(QString search)
	{
		if (search.compare(m_filter, Qt::CaseInsensitive) == 0)
			return viewed::refilter_type::same;
		else if (search.startsWith(m_filter, Qt::CaseInsensitive))
		{
			m_filter = std::move(search);
			return viewed::refilter_type::incremental;
		}
		else
		{
			m_filter = std::move(search);
			return viewed::refilter_type::full;
		}
	}

	viewed::refilter_type NotificationFilter::set_expr(NotificationLevelBitset levels)
	{
		if (m_levels == levels) 
			return viewed::refilter_type::same;

		m_levels = std::move(levels);
		return viewed::refilter_type::full;
	}

	viewed::refilter_type NotificationFilter::set_expr(NotificationPriorityBitset priorities)
	{
		if (m_priorities == priorities)
			return viewed::refilter_type::same;

		m_priorities = std::move(priorities);
		return viewed::refilter_type::full;
	}

	bool NotificationFilter::matches(const Notification & n) const
	{
		if (not m_levels.test(n.Level()))
			return false;
		
		if (not m_priorities.test(n.Priority()))
			return false;

		if (n.Title().contains(m_filter, Qt::CaseInsensitive))
			return true;

		if (n.PlainText().contains(m_filter, Qt::CaseInsensitive))
			return true;

		return false;
	}

	bool NotificationFilter::always_matches() const noexcept
	{
		return m_filter.isEmpty()
			and m_levels.all()
			and m_priorities.all();
	}

	NotificationModel::NotificationModel(std::shared_ptr<NotificationStore> store, QObject * parent /* = nullptr */)
	    : base_type(parent), view_type(std::move(store))
	{
		assert(store);

		// from view_base_type
		connect_signals();
		reinit_view();
	}

	auto NotificationModel::GetNotificationCenter() const -> QPointer<NotificationCenter>
	{
		return m_owner->GetNotificationCenter();
	}

	auto NotificationModel::GetItem(int row) const -> const Notification &
	{
		return *m_store.at(row);
	}

	void NotificationModel::Refilter()
	{
		auto rtypes = {
			m_filter_pred.set_expr(m_filterStr),
			m_filter_pred.set_expr(m_filteredLevels),
			//m_filter_pred.set_expr(m_filteredPriorities)
		};


		auto pred = [](viewed::refilter_type v1, viewed::refilter_type v2)
		{
			using type = std::underlying_type_t<viewed::refilter_type>;
			return static_cast<type>(v1) < static_cast<type>(v2);
		};

		auto rtype = std::max(rtypes, pred);
		refilter_and_notify(rtype);
	}

	int NotificationModel::FullRowCount() const
	{
		return qint(m_owner->size());
	}

	int NotificationModel::rowCount(const QModelIndex & parent /*= QModelIndex()*/) const
	{
		return qint(m_store.size());
	}

	void AbstractNotificationModel::SetFilter(QString newFilter)
	{
		return SetFiltering(std::move(newFilter), m_filteredLevels);
	}

	void AbstractNotificationModel::SetNotificationLevelFilter(NotificationLevelBitset filtered)
	{
		return SetFiltering(m_filterStr, std::move(filtered));
	}

	void AbstractNotificationModel::SetFiltering(QString expr, NotificationLevelBitset levels)
	{
		m_filterStr = std::move(expr);
		m_filteredLevels = levels;

		Refilter();

		Q_EMIT FilterChanged(m_filterStr);
		Q_EMIT NotificationLevelFilterChanged(m_filteredLevels);
	}

	QVariant AbstractNotificationModel::data(const QModelIndex & index, int role /*= Qt::DisplayRole*/) const
	{
		if (!index.isValid())
			return {};

		int row = index.row();
		int column = index.column();
		Q_UNUSED(column);

		switch (role)
		{
			//case Qt::UserRole:
			//	return QVariant::fromValue(GetItem(row));

			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				return GetItem(row).Text();

			default:          return {};
		}
	}
}
