#include <QtWidgets/QApplication>
#include "MainWindow.hqt"


int main(int argc, char * argv[])
{
	using namespace std;

	Q_INIT_RESOURCE(QtTools);

	QtTools::QtRegisterStdString();
	//QtTools::QtRegisterStdChronoTypes();	

	QApplication qapp(argc, argv);

#ifdef Q_OS_WIN
	// On windows the highlighted colors for inactive widgets are the
	// same as non highlighted colors.This is a regression from Qt 4.
	// https://bugreports.qt-project.org/browse/QTBUG-41060
	auto palette = qapp.palette();
	palette.setColor(QPalette::Inactive, QPalette::Highlight, palette.color(QPalette::Active, QPalette::Highlight));
	palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::HighlightedText));
	qapp.setPalette(palette);
#endif

	MainWindow wnd;
	wnd.show();

	return qapp.exec();
}

