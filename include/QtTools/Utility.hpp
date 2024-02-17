#pragma once
#include <QtCore/Qt>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtGui/QIcon>
#include <QtWidgets/QStyle>

QT_BEGIN_NAMESPACE
class QWidget;
class QToolBar;
class QLineEdit;
QT_END_NAMESPACE

namespace QtTools
{
	QIcon LoadIcon(const QString & themeIcon, QStyle::StandardPixmap fallback, const QStyle * style = nullptr);	
	QIcon LoadIcon(const QString & themeIcon, const QString & fallback);

	QSize ToolBarIconSizeForLineEdit(QLineEdit * lineEdit);
	
	/// Sets foreign parent(parent window from different process) for given widget
	void SetForeignParent(QWidget * widget, WId foreign_parent_winid);
}
