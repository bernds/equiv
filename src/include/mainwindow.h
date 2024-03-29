#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <cstdio>

#include <QMainWindow>
#include <QGraphicsScene>
#include <QImage>
#include <QTimer>
#include <QDialog>
#include <QSettings>
#include <QSqlDatabase>

#include "imgentry.h"

// RAII wrapper around temporarily setting m_inhibit_updates in MainWindow
class bool_changer
{
	bool &m_var;
	bool m_old;

public:
	bool_changer (bool &var, bool set) : m_var (var), m_old (var)
	{
		m_var = set;
	}
	~bool_changer ()
	{
		m_var = m_old;
	}
};

class line_terminator
{
	FILE *m_f;
public:
	line_terminator (FILE *f) : m_f (f)
	{
	}
	~line_terminator() { fputc ('\n', m_f); }
};

namespace Ui
{
	class MainWindow;
};

class ClickablePixmap;
class QActionGroup;
class QKeyEvent;

#include <QMutex>
#include <QSemaphore>
#include <QThreadPool>

class Renderer : public QObject
{
	Q_OBJECT
	QThreadPool m_pool;
public:

	// One big mutex around the drawing function
	QMutex mutex;

	QSemaphore completion_sem { 1 };

	std::atomic<bool> abort_render { false };

	void do_render ();
	void slot_render (int idx, int gen, img *, img_tweaks *, int w, int h, bool);
signals:
	void signal_render_complete (int idx, int gen);
};

class MainWindow: public QMainWindow
{
	Q_OBJECT

	Ui::MainWindow *ui;

	QThread *m_render_thread {};

	QTimer m_setup_timer;
	QTimer m_resize_timer;
	QTimer m_slide_timer;
	QTimer m_db_timer;

	QSqlDatabase m_db;
	QStringList m_db_queue;

	Renderer *m_renderer;
	bool m_render_queued = false;
	struct imgq
	{
		int idx;
		bool changed;
		bool load;
		imgq (int i, bool c = false, bool l = false) : idx (i), changed (c), load (l)
		{
		}
	};
	std::vector<imgq> m_queue;

	bool m_individual_files = false;

	QGraphicsScene m_canvas;
	QGraphicsPixmapItem *m_img {};

	// We have several scaling options: unscaled, fit width, and fit window.
	// However, "unscaled" is only unscaled by default, it actually allows
	// for free scaling using D/shift-D, as in xzgv.
	double m_img_scale = 1;
	double m_free_scale = 1;

	size_t m_cur_img_size = 0;
	int m_cur_img_nlink = 0;

	QDir m_cwd;
	std::unique_ptr<QSettings> m_img_settings;
	simple_fs_model m_model;
	int m_model_gen = 0;
	int m_idx = -1;
	int m_first_file_idx = -1;
	dir_entry *m_lru {};
	img_tweaks m_copied_tweaks;
	img_tweaks m_no_tweaks;

	bool m_sliding = false;
	int m_next_slide = -1;

	bool m_inhibit_updates = false;

	bool m_mouse_moving = false;
	int m_mouse_xbase = 0;
	int m_mouse_ybase = 0;

	void start_threads ();
	void restore_geometry ();

	void add_to_lru (dir_entry &);
	void prune_lru (int leave = 5);
	void clear_lru ();

	void update_model_gen ();

	void slide_elapsed ();
	void perform_resizes ();

	void zoom_in (bool = false);
	void zoom_out (bool = false);
	void scale_changed (int);

	void set_current_adjustments ();
	void update_wbcol_button (QColor);
	void update_tweaks_ui (const dir_entry &);
	void update_adjustments ();
	void do_autoblack (bool = false);
	void do_copy (bool = false);
	void do_paste (const img_tweaks &);

	void choose_wb_color (bool = false);
	void clear_wb (bool = false);
	void clear_black (bool = false);
	void clear_gamma (bool = false);
	void clear_brightness (bool = false);
	void clear_sat (bool = false);
	void clear_cspace (bool = false);

	void rotate (int adjust);
	void mirror (bool vertical);

	void slot_render_complete (int idx, int gen);
	void slot_save_as (bool);
	void slot_rescan (bool = false);
	void slot_rename (bool);
	void slot_delete (bool);

	void pick_wb (QMouseEvent *);
	void image_mouse_event (QMouseEvent *);
	void image_wheel_event (QWheelEvent *);

	void keyPressEvent (QKeyEvent *) override;
	void keyReleaseEvent (QKeyEvent *) override;

	void update_background ();

	void update_selection ();
	void next_image (bool);
	void prev_image (bool);
	void start_slideshow (bool);
	void stop (bool);
	void files_doubleclick ();

	void restart_render ();
	void enqueue_render (int, bool changed = false, bool load = false);

	QString load (int idx, bool queue = true);
	void load_adjustments (dir_entry &);
	QSize size_for_image (const dir_entry &, bool);
	void rescale_current ();
	bool switch_to (int idx);

	void send_tweaks_to_db (const dir_entry &);
	void sync_to_db ();

	void discard_entries ();
	int scan_cwd (QString = QString ());
	void scan (const QString &);

	void perform_setup ();

	void prefs ();
	void help_about ();
protected:
	void closeEvent(QCloseEvent *event) override;

public:
	MainWindow (const QStringList &);
	~MainWindow ();

signals:
	void signal_render (int idx, int gen, img *, img_tweaks *, int w, int h, bool);

};

#endif
