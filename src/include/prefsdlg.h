#ifndef PREFSDLG_H
#define PREFSDLG_H

#include <QDialog>
#include <QGraphicsScene>

#include <cstdint>
#include <QVector>
#include <vector>
using std::vector;
namespace Ui
{
	class PrefsDialog;
};

class color_model;
class MainWindow;
class QMouseEvent;
class QGraphicsLineItem;
class QGraphicsPixmapItem;
class QImage;

class PrefsDialog : public QDialog
{
	Q_OBJECT

	Ui::PrefsDialog *ui;

	void accept () override;

public:
	PrefsDialog (QWidget *);
};


#endif
