#include <QSettings>
#include <QComboBox>

#include "prefsdlg.h"
#include "ui_prefsdialog.h"

PrefsDialog::PrefsDialog (QWidget *parent)
	: QDialog (parent), ui (new Ui::PrefsDialog)
{
	ui->setupUi (this);

	QSettings settings;
	int style = 0;
	if (settings.contains ("mainwin/background"))
		style = settings.value ("mainwin/background").toInt ();
	ui->bgComboBox->setCurrentIndex (style);
}

void PrefsDialog::accept ()
{
	QSettings settings;
	settings.setValue ("mainwin/background", ui->bgComboBox->currentIndex ());
	QDialog::accept ();
}
