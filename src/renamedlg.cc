#include <QDir>
#include <QFile>
#include <QMessageBox>

#include "equiv.h"
#include "renamedlg.h"
#include "ui_renamedialog.h"

RenameDialog::RenameDialog (QWidget *parent, dir_entry *e)
	: QDialog (parent), m_entry (e), m_oldpath (e->path ()), ui (new Ui::RenameDialog)
{
	ui->setupUi (this);
	ui->oldNameLabel->setText (e->name);
	ui->dirLabel->setText (e->dir.path ());
	ui->newNameEdit->setText (e->name);
	ui->newNameEdit->selectAll ();
}

void RenameDialog::accept ()
{
	QString newname = ui->newNameEdit->text ();
	if (newname.isEmpty ()) {
		QMessageBox::warning (this, PACKAGE, tr ("New name is empty."));
		return;
	}
	QString cur = QDir::currentPath ();
	if (!QDir::setCurrent (m_entry->dir.absolutePath ())) {
		QMessageBox::warning (this, PACKAGE, tr ("The rename operation failed."));
		return;
	}
	QFile f (m_oldpath);
	bool success = f.rename (newname);
	QDir::setCurrent (cur);
	if (!success) {
		QMessageBox::warning (this, PACKAGE, tr ("The rename operation failed."));
		return;
	}
	QFileInfo info (f);
	m_entry->name = info.fileName ();
	m_entry->dir = info.dir ();
	QDialog::accept ();
}
