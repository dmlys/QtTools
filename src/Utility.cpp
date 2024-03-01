#include <QtCore/QTimer>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QApplication>
#include <QtTools/Utility.hpp>

#ifdef Q_OS_WIN
#include <windows.h>

#ifdef LoadIcon
#undef LoadIcon
#endif

#endif

namespace QtTools
{
	QIcon LoadIcon(const QString & themeIcon, QStyle::StandardPixmap fallback, const QStyle * style /*= nullptr*/)
	{
		if (QIcon::hasThemeIcon(themeIcon))
			return QIcon::fromTheme(themeIcon);

		if (style == nullptr)
			style = qApp->style();

		return style->standardIcon(fallback);
	}

	QIcon LoadIcon(const QString & themeIcon, const QString & fallback)
	{
		if (QIcon::hasThemeIcon(themeIcon))
			return QIcon::fromTheme(themeIcon);

		return QIcon(fallback);
	}

	QSize ToolBarIconSizeForLineEdit(QLineEdit * lineEdit)
	{
#ifdef Q_OS_WIN
		// on windows pixelMetric(QStyle::PM_DefaultFrameWidth) returns 1,
		// but for QLineEdit internal code actually uses 2
		constexpr auto frameWidth = 2;
#else
		const auto frameWidth = lineEdit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
#endif

		lineEdit->adjustSize();
		auto height = lineEdit->size().height();
		height -= frameWidth;

		return {height, height};
	}
	
	void SetForeignParent(QWidget * widget, WId foreign_parent_winid)
	{
		if (widget == nullptr or foreign_parent_winid == 0)
			return;
		
		widget->setAttribute(Qt::WA_NativeWindow);
		//assert(widget->testAttribute(Qt::WA_NativeWindow));
		QWindow * parent_window = QWindow::fromWinId(foreign_parent_winid);
		QWindow * widget_window = widget->windowHandle();
		
		if (parent_window and widget_window)
		{
			widget_window->setTransientParent(parent_window);
			// NOTE: window returned via fromWinId method above must be freed/deleted.
			//  We do this by assigning widget as QObject parent of parent_window(note: not a qwindow parent).
			//  When widget will be deleted, so will be parent_window QWindow object
			parent_window->QObject::setParent(widget);
			
		#ifdef Q_OS_WIN
			// On Windows Qt(at least on version 5.*.*) has a bug with setTransientParent.
			// 
			// WinAPI CreateWindowEx call is delayed until window is shown or some other method is called that forces window creation.
			// So if qt widget is created(without passing proper parent),
			// and immediately setTransientParent is called on it - CreateWindowEx will receive expected HWND.
			//
			// But on windows in QPA Qt in qwindowswindow.cpp in method QWindowsWindow::updateTransientParent 
			// when this widget is shown, window owner is changed nullptr via SetWindowLongPtr + GWLP_HWNDPARENT.
			// So proper HWND passed to CreateWindowEx and changed to nullptr immediately.
			// While fixing this bug it Qt is a proper fix - we can also fix it, by changing owner back via delayed call with QTimer.
			QTimer::singleShot(0, widget, [parent_window, widget_window]
			{
				HWND parentHwnd = reinterpret_cast<HWND>(parent_window->winId());
				HWND widgetHwnd = reinterpret_cast<HWND>(widget_window->winId());
				
				// If owner was changed to nullptr - Qt probably has this bug, change back to our parentHwnd.
				HWND curParentHwnd = reinterpret_cast<HWND>(GetWindowLongPtr(widgetHwnd, GWLP_HWNDPARENT));
				if (curParentHwnd == nullptr)
					SetWindowLongPtr(widgetHwnd, GWLP_HWNDPARENT, LONG_PTR(parentHwnd));
			});
		#endif
		}
	}
}
