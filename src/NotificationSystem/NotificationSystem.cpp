#include <QtTools/NotificationSystem/NotificationSystem.hqt>
#include <QtTools/NotificationSystem/NotificationSystemExt.hqt>

#include <QtCore/QStringBuilder>
#include <QtCore/QThread>
#include <QtGui/QTextDocument>
#include <QtTools/ToolsBase.hpp>

namespace QtTools::NotificationSystem
{
	SimpleNotification::SimpleNotification()
		: m_priority(Normal), m_level(Info), m_priority_inited(0), m_level_inited(0)
	{

	}

	SimpleNotification::SimpleNotification(QString title, QString text, Qt::TextFormat textFmt,  QDateTime timestamp)
	    : m_timestamp(std::move(timestamp)), m_title(std::move(title)), m_text(std::move(text)), m_textFmt(textFmt),
	      m_priority(Normal), m_level(Info), m_priority_inited(0), m_level_inited(0)
	{

	}

	static QString toPlain(const QString & rich)
	{
		QTextDocument doc;
		doc.setHtml(rich);

		return doc.toPlainText();
	}

	static QString toPlain(const QString & text, Qt::TextFormat fmt)
	{
		switch (fmt)
		{
			case Qt::PlainText: return text;
			case Qt::RichText:  return toPlain(text);

			case Qt::AutoText:
			default:
				if (Qt::mightBeRichText(text))
					return toPlain(text);
				else
					return text;
		}
	}

	QString SimpleNotification::PlainText() const
	{
		return toPlain(m_text, m_textFmt);
	}

	QString SimpleNotification::PlainFullText() const
	{
		return toPlain(m_fullText, m_fullTextFmt);
	}

	QString SimpleNotification::FullText() const
	{
		return m_fullText.isNull() ? m_text : m_fullText;
	}

	QString SimpleNotification::FullText(QString fullText)
	{
		return std::exchange(m_fullText, std::move(fullText));
	}

	Qt::TextFormat SimpleNotification::FullTextFmt() const
	{
		return m_fullText.isNull() ? m_fullTextFmt : m_fullTextFmt;
	}

	Qt::TextFormat SimpleNotification::FullTextFmt(Qt::TextFormat fmt)
	{
		return std::exchange(m_fullTextFmt, std::move(fmt));
	}

	auto SimpleNotification::Priority() const -> NotificationPriority
	{
		return static_cast<NotificationPriority>(m_priority);
	}

	auto SimpleNotification::Priority(NotificationPriority priority) -> NotificationPriority
	{
		auto ret = m_priority;
		m_priority = priority;
		m_priority_inited = 1;

		return static_cast<NotificationPriority>(ret);
	}

	auto SimpleNotification::Level() const -> NotificationLevel
	{
		return static_cast<NotificationLevel>(m_level);
	}

	auto SimpleNotification::Level(NotificationLevel level) -> NotificationLevel
	{
		auto ret = m_level;
		m_level = level;
		m_level_inited = 1;

		if (level == Error and not m_priority_inited)
			m_priority = High;

		return static_cast<NotificationLevel>(ret);
	}

	NotificationCenter::NotificationCenter(QObject * parent /* = nullptr */)
		: QObject(parent)
	{
		m_store = std::make_shared<NotificationStore>(this);
	}

	auto NotificationCenter::GetStore() -> std::shared_ptr<NotificationStore>
	{
		return m_store;
	}

	auto NotificationCenter::GetStore() const -> std::shared_ptr<const NotificationStore>
	{
		return m_store;
	}

	void NotificationCenter::DoAddNotification(const Notification * notification)
	{
		m_store->push_back(notification);
		Q_EMIT NotificationAdded(m_store->back());
	}

	auto NotificationCenter::CreateNotification() const
		-> std::unique_ptr<Notification>
	{
		return std::make_unique<SimpleNotification>();
	}

	void NotificationCenter::AddNotification(std::unique_ptr<const Notification> notification)
	{		
		auto * thr = this->thread();
		if (thr and thr == thr->currentThread())
		{
			return DoAddNotification(notification.release());
		}
		else
		{
			QMetaObject::invokeMethod(this, "DoAddNotification", Qt::QueuedConnection,
				Q_ARG(const Notification *, notification.get()));

			notification.release();
		}
	}

	void NotificationCenter::AddInfo(QString title, QString text, Qt::TextFormat fmt /* = Qt::AutoText */, QDateTime timestamp /* = QDateTime::currentDateTime() */)
	{
		auto notification = std::make_unique<SimpleNotification>(std::move(title), std::move(text), fmt, std::move(timestamp));
		notification->Level(Info);

		AddNotification(std::move(notification));
	}

	void NotificationCenter::AddWarning(QString title, QString text, Qt::TextFormat fmt /* = Qt::AutoText */, QDateTime timestamp /* = QDateTime::currentDateTime() */)
	{
		auto notification = std::make_unique<SimpleNotification>(std::move(title), std::move(text), fmt, std::move(timestamp));
		notification->Level(Warn);

		AddNotification(std::move(notification));
	}

	void NotificationCenter::AddError(QString title, QString text, Qt::TextFormat fmt /* = Qt::AutoText */, QDateTime timestamp /* = QDateTime::currentDateTime() */)
	{
		auto notification = std::make_unique<SimpleNotification>(std::move(title), std::move(text), fmt, std::move(timestamp));
		notification->Level(Error);

		AddNotification(std::move(notification));
	}
}
