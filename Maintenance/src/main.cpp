#include <qapplication.h>

#include "MaintenanceWindow.h"


int main(int argc, char** argv)
{
	QApplication* maint;
	maint = new QApplication(argc, argv);

    QFont font("Verdana", 8);
    qApp->setFont(font);

	MaintenanceWindow* maintWindow = new MaintenanceWindow();
	maintWindow->setVisible(true);

	return maint->exec();
}
