#ifndef RENAMEDLG_H
#define RENAMEDLG_H

#include <QDialog>
#include "imgentry.h"

namespace Ui
{
	class RenameDialog;
};

class QDir;

class RenameDialog : public QDialog
{
	Q_OBJECT

	dir_entry *m_entry;
	QString m_oldpath;
	Ui::RenameDialog *ui;

	void accept () override;
public:
	RenameDialog (QWidget *, dir_entry *);
};


#endif
