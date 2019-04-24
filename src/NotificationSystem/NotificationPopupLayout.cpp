#include <tuple>
#include <ext/config.hpp>
#include <ext/utility.hpp>

#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtTools/NotificationSystem/NotificationSystem.hqt>
#include <QtTools/NotificationSystem/NotificationPopupLayout.hqt>
#include <QtTools/NotificationSystem/NotificationPopupLayoutExt.hqt>


namespace QtTools::NotificationSystem
{	
	const NotificationPopupLayout::CreatePopupFunction NotificationPopupLayout::ms_defaultCreatePopup = 
		[](const Notification & n, const NotificationPopupLayout & that) { return that.CreatePopup(n); };
	
	NotificationPopupLayout::Item::Item(Item && other) = default;


	auto NotificationPopupLayout::Item::operator=(Item && other) -> Item &
	{
		if (this != &other)
		{
			this->~Item();
			new (this) Item(std::move(other));
		}

		return *this;
	}

	NotificationPopupLayout::Item::~Item()
	{
		delete slideAnimation.data();
		delete moveOutAnimation.data();

		if (auto * popup = this->popup.data())
		{
			popup->disconnect(popup->GetNotificationPopupLayout());
			delete popup;
		}
	}

	NotificationPopupLayout::~NotificationPopupLayout() = default;


	void NotificationPopupLayout::InitColors()
	{
		m_errorColor = QColor("red");
		m_errorColor.setAlpha(200);

		m_warnColor = QColor("yellow");
		m_warnColor.setAlpha(200);

		m_infoColor = QColor("silver");
		m_infoColor.setAlpha(200);
	}

	NotificationPopupLayout::NotificationPopupLayout(QObject * parent /* = nullptr */)
		: QObject(parent)
	{
		InitColors();
	}

	NotificationPopupLayout::NotificationPopupLayout(NotificationCenter & center, QObject * parent /* = nullptr */)
		: NotificationPopupLayout(parent)
	{
		Init(center);
	}

	void NotificationPopupLayout::Init(NotificationCenter & center)
	{
		SetNotificationCenter(&center);		
	}

	void NotificationPopupLayout::SetNotificationCenter(NotificationCenter * center)
	{
		if (m_ncenter) m_ncenter->disconnect(this);
		
		m_ncenter = center;

		if (m_ncenter)
		{
			connect(m_ncenter, &NotificationCenter::NotificationAdded,
			        this, static_cast<void(NotificationPopupLayout::*)(QPointer<const Notification>)>(&NotificationPopupLayout::AddNotification));
		}
	}

	void NotificationPopupLayout::SetCreatePopupFunction(CreatePopupFunction func)
	{
		if (not func)
		{
			m_createPopup = ms_defaultCreatePopup;
			return;
		}

		m_createPopup = std::move(func);
	}

	unsigned NotificationPopupLayout::NotificationsCount() const
	{
		return static_cast<unsigned>(m_items.size());
	}

	void NotificationPopupLayout::AddNotification(QPointer<const Notification> notification)
	{
		Item item;
		item.notification = std::move(notification);

		m_items.push_back(std::move(item));		
		ScheduleUpdate();
	}

	void NotificationPopupLayout::AddPopup(AbstractNotificationPopupWidget * popup)
	{
		Item item;
		item.popup = popup;

		popup->hide();
		popup->setAttribute(Qt::WA_DeleteOnClose, true);
		popup->setParent(m_parent);
		popup->adjustSize();

		popup->installEventFilter(this);
		popup->SetNotificationPopupLayout(this);

		connect(popup, &QObject::destroyed, this, &NotificationPopupLayout::NotificationPopupClosed);
		connect(popup, &AbstractNotificationPopupWidget::LinkActivated, this, &NotificationPopupLayout::LinkActivated);
		connect(popup, &AbstractNotificationPopupWidget::LinkHovered, this, &NotificationPopupLayout::LinkHovered);

		m_items.push_back(std::move(item));
		ScheduleUpdate();
	}

	void NotificationPopupLayout::ScheduleUpdate()
	{
		if (not m_relayoutScheduled)
		{
			QMetaObject::invokeMethod(this, "DoScheduledUpdate", Qt::QueuedConnection);
			m_relayoutScheduled = true;
		}
	}

	void NotificationPopupLayout::DoScheduledUpdate()
	{
		Update(m_relocation);
		m_relayoutScheduled = false;
		m_relocation = false;
	}

	auto NotificationPopupLayout::DescribeCorner(Qt::Corner corner) -> std::tuple<GetPointPtr, MovePointPtr, int>
	{
		switch (corner)
		{
			case Qt::TopLeftCorner:
				return std::make_tuple(&QRect::topLeft, &QRect::moveTopLeft, +1);

			case Qt::TopRightCorner:
				return std::make_tuple(&QRect::topRight, &QRect::moveTopRight, +1);

			case Qt::BottomLeftCorner:
				return std::make_tuple(&QRect::bottomLeft, &QRect::moveBottomLeft, -1);

			case Qt::BottomRightCorner:
			default: // treat anything other as BottomRightCorner
				return std::make_tuple(&QRect::bottomRight, &QRect::moveBottomRight, -1);
		}
	}

	QRect NotificationPopupLayout::AlignRect(QRect rect, const QRect & parent, Qt::Corner corner)
	{
		auto [getter, setter, direction] = DescribeCorner(corner);
		Q_UNUSED(direction);

		(rect.*setter)((parent.*getter)());
		return rect;
	}

	QRect NotificationPopupLayout::DefaultLayoutRect(const QRect & parent, Qt::Corner corner) const
	{
		QFont font = qApp->font("QAbstractItemView");
		QFontMetrics fm(font);

		// calculate default rect based on parent rect and corner
		const auto minimumWidth = fm.averageCharWidth() * 40;
		const auto maximumWidth = fm.averageCharWidth() * 60;

		// minimum height - at least 4 rows of text(1 for title + 3 for main text) 
		// with some additional spacing
		const auto minimumHeight = fm.height() * 4 + 20;
		const auto maximumHeight = parent.height();

		auto width = parent.width() / 3;
		auto height = parent.height() / 3;

		width = qBound(minimumWidth, width, maximumWidth);
		height = qBound(minimumHeight, height, maximumHeight);

		auto [getter, setter, direction] = DescribeCorner(corner);
		Q_UNUSED(direction);

		QRect rect(0, 0, width, height);
		(rect.*setter)((parent.*getter)());

		return rect;
	}

	QRect NotificationPopupLayout::ParentRect() const
	{
		if (m_parent)
			return m_parent->rect();
		else
		{
			auto * desktop = qApp->desktop();
			return desktop->availableGeometry(desktop->primaryScreen());
		}
	}


	QRect NotificationPopupLayout::CalculateLayoutRect() const
	{
		QRect parentRect = ParentRect();

		if (not m_geometry.isNull())
			return AlignRect(m_geometry, parentRect, m_corner);
		else
			return DefaultLayoutRect(parentRect, m_corner);
	}

	auto NotificationPopupLayout::CreatePopup(const Notification & notification) const
		-> AbstractNotificationPopupWidget *
	{
		// try create using Q_INVOKABLE createPopup method of notification,
		// if failed fallback to NotificationPopupWidget.
		auto * meta = notification.metaObject();
		int index = meta->indexOfMethod("createPopup(const QtTools::NotificationSystem::NotificationPopupLayout &)");
		if (index >= 0)
		{
			AbstractNotificationPopupWidget * result = nullptr;
			QMetaMethod method = meta->method(index);
			if (method.invoke(ext::unconst(&notification), Qt::DirectConnection,
			                  Q_RETURN_ARG(QtTools::NotificationSystem::AbstractNotificationPopupWidget *, result),
			                  Q_ARG(const QtTools::NotificationSystem::NotificationPopupLayout &, *this)))
			{
				return result;
			}
		}
		
		// NOTE: NotificationPopupWidget default constructor has Qt::ToolTip window flag
		auto * widget = new NotificationPopupWidget(notification);
		CustomizePopup(notification, widget);
		ConfigureExpiration(notification, widget);
		return widget;
	}

	void NotificationPopupLayout::CustomizePopup(const Notification & n, AbstractNotificationPopupWidget * popup) const
	{	
		if (auto color = qvariant_cast<QColor>(n.property("backgroundColor")); color.isValid())
			popup->SetBackgroundBrush(color);
		else if (auto brush = qvariant_cast<QBrush>(n.property("backgroundBrush")); brush != Qt::NoBrush)
			popup->SetBackgroundBrush(brush);
		else
		{
			constexpr auto getter = [](auto val) { return val; };
			unsigned lvl = n.Level() - NotificationLevel::Error;

			auto colors = GetColors();
			auto color = ext::visit(colors, lvl, getter);
			popup->SetBackgroundBrush(color);
		}
	}

	void NotificationPopupLayout::ConfigureExpiration(const Notification & n, AbstractNotificationPopupWidget * popup) const
	{
		int msecs = 0;

		auto prop = n.property("expirationTimeout");
		if (prop.canConvert<int>())
			msecs = qvariant_cast<int>(prop);
		else
		{
			constexpr auto getter = [](auto val) { return val; };
			unsigned lvl = n.Level() - NotificationLevel::Error;

			auto timeouts = GetExpirationTimeouts();
			std::chrono::milliseconds timeout = ext::visit(timeouts, lvl, getter);
			msecs = timeout.count();
		}

		if (msecs != 0)
		{
			auto moveOut = [popup, that = ext::unconst(this)]() mutable
			{
				auto first = that->m_items.begin();
				auto last = that->m_items.end();
				auto it = std::find_if(first, last, [popup](auto & item) { return item.popup == popup; });
				if (it == last) return;
				
				that->MoveOutPopup(*it);
			};

			QTimer::singleShot(msecs, popup, moveOut);
		}
			
	}

	auto NotificationPopupLayout::MakePopup(const Notification * notification) const 
		-> AbstractNotificationPopupWidget *
	{
		auto * popup = m_createPopup(*notification, *this);
		popup->hide();
		popup->setAttribute(Qt::WA_DeleteOnClose, true);
		popup->setParent(m_parent);
		popup->adjustSize();

		popup->installEventFilter(ext::unconst(this));
		popup->SetNotificationPopupLayout(ext::unconst(this));

		connect(popup, &QObject::destroyed, this, &NotificationPopupLayout::NotificationPopupClosed);
		connect(popup, &AbstractNotificationPopupWidget::LinkActivated, this, &NotificationPopupLayout::LinkActivated);
		connect(popup, &AbstractNotificationPopupWidget::LinkHovered,   this, &NotificationPopupLayout::LinkHovered);
		
		return popup;
	}

	void NotificationPopupLayout::NotificationPopupClosed()
	{
		auto * sender = QObject::sender();
		if (not sender) return;

		auto first = m_items.begin();
		auto last = m_items.end();
		auto it = std::find_if(first, last, [sender](auto & item) { return item.popup == sender; });
		if (it == last) return;

		it->popup = nullptr;
		m_items.erase(it);

		ScheduleUpdate();
	}

	void NotificationPopupLayout::SlidePopup(Item & item, const QRect & hgeom, const QRect & lgeom) const
	{
		auto * animation = item.slideAnimation.data();
		if (animation)
		{
			if (animation->state() == animation->Running)
			{
				animation->pause();
				animation->setStartValue(hgeom);
				animation->setEndValue(lgeom);
				animation->resume();
			}			
		}
		else
		{
			auto * popup = item.popup.data();
			animation = new QPropertyAnimation(popup, "geometry", popup);
			animation->setEasingCurve(QEasingCurve::InCirc);
			animation->setStartValue(hgeom);
			animation->setEndValue(lgeom);

			animation->start(animation->DeleteWhenStopped);
			item.slideAnimation = animation;
		}
	}

	void NotificationPopupLayout::MoveOutPopup(Item & item)
	{
		// already have moving out
		if (item.moveOutAnimation) return;

		if (auto * slideAnimation = item.slideAnimation.data())
		{
			connect(slideAnimation, &QAbstractAnimation::destroyed,
			        item.popup.data(), [&item, this] { item.slideAnimation = nullptr; MoveOutPopup(item); });
			
			return;
		}

		item.moveOutAnimation = item.popup->MoveOutAndClose();
	}

	void NotificationPopupLayout::Update(bool relocation)
	{
		Relayout(relocation);
	}

	void NotificationPopupLayout::Relayout(bool relocation)
	{
		// see DescribeCorner description
		auto [getter, setter, direction] = DescribeCorner(m_corner);
		auto geometry = CalculateLayoutRect();

		if (relocation)
		{
			// slide out widgets should be removed
			auto pred = [this](auto & item) 
			{ 
				if (not item.moveOutAnimation) 
					return false; 
				else
				{
					item.popup->disconnect(this);
					return true;
				}
			};

			auto first = m_items.begin();
			auto last = m_items.end();
			first = std::remove_if(first, last, pred);
			m_items.erase(first, last);

			// if relocation - delete slide animations
			for (auto & item : m_items)
				delete item.slideAnimation;
		}


		// hcur/lcur, hgeom/lgeom - high/low.
		// When relayouting widgets, some of them can be already closed,
		// and following ones should be slided up to down(logically, corner can invert those meanings).
		// So we have 2 current points:
		//   * low  - where current widget should end after sliding
		//   * high - where current widget is currently placed.
		//
		// New widgets are always created at high position.
		// If there were no gaps - low and high will be same.
		// 
		// NOTE: Currently moving out widgets are sort of anchors,
		//       they reset both low and high position to their current position.
		//       Thats because we do not know nature of MoveOut animation,
		//       it can change geometry of a widget or not

		unsigned curWidgetIndex = 0;
		QPoint start = (geometry.*getter)();
		QPoint lcur = start;
		QPoint hcur = start;

		QRect hgeom, lgeom;
		QSize popupSz;

		auto first = m_items.begin();
		auto last  = m_items.end();

		for (; first != last; ++first)
		{
			bool forced_next = false;
			if (relocation)
			{
				auto it = std::next(first);
				forced_next = it != last and it->popup and not it->popup->isHidden();
			}

			if (not forced_next and curWidgetIndex++ >= m_widgetsLimit)
				break;

			auto & item = *first;
			auto * popup = item.popup.data();
			bool is_new = not popup;

			// if relocation in place - item.moveOutAnimation == nullptr always
			if (item.moveOutAnimation)
			{
				// popup is moving out, we should reset our y coordinate to one of this widget
				auto geom = popup->geometry();
				popupSz = geom.size();
				hcur.ry() = lcur.ry() = (geom.*getter)().y();

				goto loop_end;
			}
 
			if (is_new)
			{
				item.popup = popup = MakePopup(item.notification);
				popup->show();
			}
						
			if (auto hint = popup->heightForWidth(geometry.width()); hint < 0)
			{
				popupSz = popup->size();
				popupSz.rwidth() = geometry.width();
			}
			else
			{
				popupSz.rwidth() = geometry.width();
				popupSz.rheight() = hint;
			}

			// always add spacing first
			lcur.ry() += direction * ms_spacing;
			hcur.ry() += direction * ms_spacing;

			lgeom.setRect(0, 0, popupSz.width(), popupSz.height());
			(lgeom.*setter)(lcur);

			if (is_new)
			{
				// new popup is always placed in hgeom
				hgeom.setRect(0, 0, popupSz.width(), popupSz.height());
				(hgeom.*setter)(hcur);

				popup->setGeometry(hgeom);
			}
			else if (relocation)
			{
				popup->setGeometry(lgeom);
				// hgeom = lgeom;
			}
			else
			{
				hgeom = popup->geometry();
				hcur = (hgeom.*getter)();
			}
			
			if (hcur != lcur)
				SlidePopup(item, hgeom, lgeom);

		loop_end:
			lcur.ry() += direction * popupSz.height();
			hcur.ry() += direction * popupSz.height();

			// if available geometry exhausted
			if (not forced_next and std::abs(lcur.y() - start.y()) >= geometry.height())
				break;
		}
	}

	void NotificationPopupLayout::ParentResized()
	{
		m_relocation = true;
		ScheduleUpdate();
	}

	bool NotificationPopupLayout::eventFilter(QObject * watched, QEvent * event)
	{
		auto evType = event->type();
		if (evType == QEvent::Resize)
		{
			//auto * rev = static_cast<QResizeEvent *>(event);
			ParentResized();
			return false;
		}	
		
		bool mouseButtonEvent =
			   evType == QEvent::MouseButtonPress
			or evType == QEvent::MouseButtonDblClick;

		if (not mouseButtonEvent)
			return false;

		auto first = m_items.begin();
		auto last = m_items.end();
		auto it = std::find_if(first, last, [watched](auto & item) { return item.popup == watched; });
		if (it == last) return false;

		if (evType == QEvent::MouseButtonDblClick)
		{
			auto * notification = it->notification.data();
			if (not notification) return true;
			
			auto href = notification->ActivationLink();
			if (href.isEmpty()) return true;

			LinkActivated(std::move(href));
			MoveOutPopup(*it);
			return true;
		}

		auto * ev = static_cast<QMouseEvent *>(event);
		if (ev->button() != Qt::RightButton) return false;
		
		auto * widget = it->popup.data();

		if (not widget->contentsRect().contains(ev->pos()))
			return false;

		MoveOutPopup(*it);
		return true;
	}

	void NotificationPopupLayout::SetParent(QWidget * widget)
	{
		if (m_parent)
			m_parent->removeEventFilter(this);
		else
		{
			for (auto * screen : qApp->screens())
				screen->disconnect(this);
		}

		m_parent = widget;
		m_relocation = true;

		if (m_parent)
			m_parent->installEventFilter(this);
		else
		{
			QScreen * screen = qApp->primaryScreen();
			connect(screen, &QScreen::availableGeometryChanged,
			        this, &NotificationPopupLayout::ParentResized);
		}

		ScheduleUpdate();
	}

	void NotificationPopupLayout::SetGeometry(const QRect & geom)
	{
		m_geometry = geom;
		m_relocation = true;
		ScheduleUpdate();
	}

	void NotificationPopupLayout::SetCorner(Qt::Corner corner)
	{
		m_corner = corner;
		ScheduleUpdate();
	}

	void NotificationPopupLayout::SetWidgetsLimit(unsigned limit)
	{
		m_widgetsLimit = limit;
	}

	auto NotificationPopupLayout::GetColors() const -> std::tuple<QColor, QColor, QColor>
	{
		return std::make_tuple(m_errorColor, m_warnColor, m_infoColor);
	}

	void NotificationPopupLayout::SetColors(QColor error, QColor warning, QColor info)
	{
		m_errorColor = error;
		m_warnColor = warning;
		m_infoColor = info;
	}

	auto NotificationPopupLayout::GetExpirationTimeouts() const
		-> std::tuple<std::chrono::milliseconds, std::chrono::milliseconds, std::chrono::milliseconds>
	{
		return std::make_tuple(m_errorTimeout, m_warnTimeout, m_infoTimeout);
	}

	void NotificationPopupLayout::SetExpirationTimeouts(std::chrono::milliseconds error, std::chrono::milliseconds warning, std::chrono::milliseconds info)
	{
		m_errorTimeout = error;
		m_warnTimeout = warning;
		m_infoTimeout = info;
	}
}
