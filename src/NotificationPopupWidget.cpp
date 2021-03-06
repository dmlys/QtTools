#include <QtTools/NotificationPopupWidget.hqt>

#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <QtCore/QPropertyAnimation>

// src/widgets/effects/qpixmapfilter.cpp
Q_DECL_IMPORT void qt_blurImage(
	QPainter * p, QImage & blurImage,
	qreal radius, bool quality, bool alphaOnly, int transposed = 0
);

namespace QtTools
{
	QMarginsF NotificationPopupWidget::ShadowMargins() const noexcept
	{
		auto blur_radius = m_effect->blurRadius();
		auto blur_offset = m_effect->offset();

		return {
			std::max(blur_radius - blur_offset.x(), 0.0),
			std::max(blur_radius - blur_offset.y(), 0.0),
			blur_radius + blur_offset.x(),
			blur_radius + blur_offset.y(),
		};
	}

	QMargins NotificationPopupWidget::FrameMargins() const noexcept
	{
		auto frame_width = m_framePen.width();
		return {frame_width, frame_width, frame_width, frame_width};
	}

	QRectF NotificationPopupWidget::DropShadowEffect::boundingRectFor(const QRectF & rect) const
	{
		// this is original calculation
		//auto radius = blurRadius();
		//return rect | rect.translated(offset()).adjusted(-radius, -radius, radius, radius);
		
		// but we return rect as is(see NotificationPopupWidget description)
		return rect;
	}

	void NotificationPopupWidget::DropShadowEffect::draw(QPainter * painter)
	{
		if (blurRadius() <= 0 && offset().isNull())
		{
			drawSource(painter);
			return;
		}

		// Draw pixmap in device coordinates to avoid pixmap scaling.
		QPoint offset;
		const QPixmap pixmap = sourcePixmap(Qt::DeviceCoordinates, &offset, PadToEffectiveBoundingRect);
		if (pixmap.isNull())
			return;

		QTransform restoreTransform = painter->worldTransform();
		painter->setWorldTransform(QTransform());

		// NOTE: pixmap contains our whole widget rect.
		//   rect = ShadowMargins + FrameMargins + contentsRect
		auto * owner = this->owner();
		auto margins = owner->contentsMargins();
		auto frameMargins = owner->FrameMargins();

		// draw pixmap into tmp for further processing(blurring etc)
		QImage tmp(pixmap.size(), QImage::Format_ARGB32_Premultiplied);
		tmp.setDevicePixelRatio(pixmap.devicePixelRatioF());
		tmp.fill(0);
		QPainter tmpPainter(&tmp);
		tmpPainter.setCompositionMode(QPainter::CompositionMode_Source);
		tmpPainter.drawPixmap(this->offset(), pixmap);
		tmpPainter.end();

		// blur the alpha channel from tmp into blurPainter(blurred)
		QImage blurred(tmp.size(), QImage::Format_ARGB32_Premultiplied);
		blurred.setDevicePixelRatio(pixmap.devicePixelRatioF());
		blurred.fill(0);
		QPainter blurPainter(&blurred);
		qt_blurImage(&blurPainter, tmp, blurRadius(), false, true);
		blurPainter.end();

		tmp = blurred;

		// blacken blurred shadow image
		tmpPainter.begin(&tmp);
		tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
		tmpPainter.fillRect(tmp.rect(), color());
		tmpPainter.end();

		// draw the blurred drop shadow into resulting painter
		painter->drawImage(offset, tmp);

		// Draw the actual pixmap, but only contents + frame
		auto old = painter->compositionMode();
		painter->setCompositionMode(QPainter::CompositionMode_Source);
		painter->drawPixmap(offset, pixmap, pixmap.rect() - margins + frameMargins);
		painter->setCompositionMode(old);

		painter->setWorldTransform(restoreTransform);
	}

	QSize NotificationPopupWidget::sizeHint() const
	{
		auto margins = contentsMargins();
		
		return QWidget::sizeHint() + QSize {
			margins.left() + margins.right(),
			margins.top() + margins.bottom(),
		};
	}

	void NotificationPopupWidget::paintEvent(QPaintEvent * ev)
	{
		QPainter painter(this);

		painter.setPen(m_framePen);
		painter.setBrush(m_backgroundBrush);
		
		auto frameWidth = m_framePen.width();
		painter.drawRect(contentsRect().adjusted(-frameWidth, -frameWidth, 0, 0));
	}

	void NotificationPopupWidget::mousePressEvent(QMouseEvent * ev)
	{
		if (ev->button() == Qt::RightButton and contentsRect().contains(ev->pos()))
			MoveOutAndClose();

		return base_type::mousePressEvent(ev);
	}

	QAbstractAnimation * NotificationPopupWidget::CreateMoveOutAnimation()
	{
		auto * animation = new QPropertyAnimation(this, "geometry", this);

		QRectF parentRect;
		auto * parent = this->parentWidget();
		if (parent)
			parentRect = parent->rect();
		else
			parentRect = qApp->desktop()->availableGeometry(this);

		QRectF start = this->geometry();
		QSizeF size = start.size();
		QRectF finish = parentRect.right() - start.right() < start.left() - parentRect.left()
			? QRectF(QPointF(parentRect.right(), start.top()), size)
			: QRectF(QPointF(parentRect.left() - size.width(), start.top()), size);

		animation->setStartValue(start);
		animation->setEndValue(finish);
		animation->setEasingCurve(QEasingCurve::InCirc);

		return animation;
	}

	QAbstractAnimation * NotificationPopupWidget::MoveOutAndClose()
	{
		auto * animation = CreateMoveOutAnimation();
		connect(animation, &QPropertyAnimation::finished, this, &NotificationPopupWidget::close);
		connect(animation, &QPropertyAnimation::finished, this, &NotificationPopupWidget::MovedOut);

		animation->setParent(this);
		animation->start(animation->DeleteWhenStopped);

		return animation;
	}

	NotificationPopupWidget::NotificationPopupWidget(QWidget * parent /* = nullptr */, Qt::WindowFlags flags /* = {} */)
		: QWidget(parent, flags)
	{
		// normally notifications are not needed after they are closed
		setAttribute(Qt::WA_DeleteOnClose);
		setWindowFlag(Qt::FramelessWindowHint);
		// in paintEvent rect is drawn in entire clientRect, so we are - opaque
		setAttribute(Qt::WA_OpaquePaintEvent);

		if (isWindow())
			setAttribute(Qt::WA_TranslucentBackground);

		auto * shadowEffect = new DropShadowEffect(this);
		shadowEffect->setBlurRadius(4.0);
		shadowEffect->setColor(palette().color(QPalette::Shadow));
		shadowEffect->setOffset(4.0);
		this->setGraphicsEffect(shadowEffect);
		
		m_effect = shadowEffect;
		
		auto margins = ShadowMargins() + FrameMargins();
		setContentsMargins(margins.toMargins());
	}

	QPointF NotificationPopupWidget::ShadowOffset() const noexcept
	{
		return m_effect->offset();
	}

	void NotificationPopupWidget::SetShadowOffset(QPointF offset)
	{
		m_effect->setOffset(offset);
		setContentsMargins((ShadowMargins() + FrameMargins()).toMargins());
		Q_EMIT ShadowOffsetChanged(offset);
		update();
	}

	void NotificationPopupWidget::ResetShadowOffset()
	{
		SetShadowOffset({4.0, 4.0});
	}

	qreal NotificationPopupWidget::ShadowBlurRadius() const noexcept
	{
		return m_effect->blurRadius();
	}

	void NotificationPopupWidget::SetShadowBlurRadius(qreal radius)
	{
		m_effect->setBlurRadius(radius);
		setContentsMargins((ShadowMargins() + FrameMargins()).toMargins());
		Q_EMIT ShadowBlurRadiusChanged(radius);
	}

	void NotificationPopupWidget::ResetShadowBlurRadius()
	{
		SetShadowBlurRadius(4.0);
	}

	QColor NotificationPopupWidget::ShadowColor() const noexcept
	{
		return m_effect->color();
	}

	void NotificationPopupWidget::SetShadowColor(QColor color)
	{
		m_effect->setColor(color);
		Q_EMIT ShadowColorChanged(color);
	}

	void NotificationPopupWidget::ResetShadowColor()
	{
		SetShadowColor(palette().color(QPalette::Shadow));
	}

	QPen NotificationPopupWidget::FramePen() const noexcept
	{
		return m_framePen;
	}

	void NotificationPopupWidget::SetFramePen(QPen pen)
	{
		m_framePen = pen;
		setContentsMargins((ShadowMargins() + FrameMargins()).toMargins());
		Q_EMIT FramePenChanged(std::move(pen));
		update();
	}

	void NotificationPopupWidget::ResetFramePen()
	{
		SetFramePen(palette().color(QPalette::Shadow));
	}

	QBrush NotificationPopupWidget::BackgroundBrush() const noexcept
	{
		return m_backgroundBrush;
	}

	void NotificationPopupWidget::SetBackgroundBrush(QBrush brush)
	{
		m_backgroundBrush = brush;
		Q_EMIT BackgroundBrushChanged(std::move(brush));
		update();
	}

	void NotificationPopupWidget::ResetBackgroundBrush()
	{
		SetBackgroundBrush(QBrush());
	}
}
