#include <cstdint>
#include <cassert>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <algorithm>

#include <time.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QColorDialog>
#include <QProgressDialog>
#include <QVector>
#include <QFileDialog>
#include <QSettings>
#include <QThread>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QMessageBox>
#include <QTimer>
#include <QMouseEvent>
#include <QDataStream>
#include <QRunnable>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QDebug>
#include <QRegularExpression>
#include <QScrollBar>

#include "equiv.h"
#include "colors.h"
#include "imgentry.h"
#include "util-widgets.h"

#include "prefsdlg.h"
#include "renamedlg.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

img::~img ()
{
}

QString img_tweaks::to_string () const
{
	QString str;
	if (blacklevel != 0)
		str += "bk:" + QString::number (blacklevel) + ";";
	if (gamma != 0)
		str += "g:" + QString::number (gamma) + ";";
	if (white != Qt::white)
		str += "wb:" + QString::number (white.red ()) + "," + QString::number (white.green ())+ "," + QString::number (white.blue ()) + ";";
	if (sat != 0)
		str += "s:" + QString::number (sat) + ";";
	if (brightness != 0)
		str += "br:" + QString::number (brightness) + ";";
	if (rot != 0)
		str += "rot:" + QString::number (rot) + ";";
	if (mirrored != 0)
		str += "mir;";
	if (cspace_idx != 0)
		str += "cs:" + QString::number (cspace_idx) + ";";
	str += unknown_tags;
	return str;
}

// Return true for obsolete forms that we should rewrite.
bool img_tweaks::from_string (QString s)
{
	static QRegularExpression re1 ("(\\d+),(-?\\d+),(\\d+),(\\d+),(\\d+),(-?\\d+),(-?\\d+)");
	static QRegularExpression re2 ("(\\d+),(-?\\d+),(\\d+),(\\d+),(\\d+),(-?\\d+)");
	static QRegularExpression re3 ("(\\d+),(-?\\d+),(\\d+),(\\d+),(\\d+)");

	auto result1 = re1.match (s);
	if (result1.hasMatch ()) {
		blacklevel = result1.captured (1).toInt ();
		gamma = result1.captured (2).toInt ();
		white.setRed (result1.captured (3).toInt ());
		white.setGreen (result1.captured (4).toInt ());
		white.setBlue (result1.captured (5).toInt ());
		sat = result1.captured (6).toInt ();
		brightness = result1.captured (7).toInt ();
		unknown_tags = QString ();
		return true;
	}
	auto result2 = re2.match (s);
	if (result2.hasMatch ()) {
		blacklevel = result2.captured (1).toInt ();
		gamma = result2.captured (2).toInt ();
		white.setRed (result2.captured (3).toInt ());
		white.setGreen (result2.captured (4).toInt ());
		white.setBlue (result2.captured (5).toInt ());
		sat = result2.captured (6).toInt ();
		unknown_tags = QString ();
		return true;
	}
	auto result3 = re3.match (s);
	if (result3.hasMatch ()) {
		blacklevel = result3.captured (1).toInt ();
		gamma = result3.captured (2).toInt ();
		white.setRed (result3.captured (3).toInt ());
		white.setGreen (result3.captured (4).toInt ());
		white.setBlue (result3.captured (5).toInt ());
		sat = 0;
		unknown_tags = QString ();
		return true;
	}

	static QRegularExpression re_b ("bk:(\\d+);");
	static QRegularExpression re_wb ("wb:(\\d+),(\\d+),(\\d+);");
	static QRegularExpression re_sat ("s:(-?\\d+);");
	static QRegularExpression re_gamma ("g:(-?\\d+);");
	static QRegularExpression re_brite ("br:(-?\\d+);");
	static QRegularExpression re_rot ("rot:(\\d+);");
	static QRegularExpression re_mir ("mir;");
	static QRegularExpression re_cs ("cs:(\\d+);");
	auto result_b = re_b.match (s);
	auto result_wb = re_wb.match (s);
	auto result_sat = re_sat.match (s);
	auto result_gamma = re_gamma.match (s);
	auto result_brite = re_brite.match (s);
	auto result_rot = re_rot.match (s);
	auto result_mir = re_mir.match (s);
	auto result_cs = re_cs.match (s);
	if (result_b.hasMatch ())
		blacklevel = result_b.captured (1).toInt ();
	if (result_sat.hasMatch ()
	    /* Using a single letter "s:" was a poor choice, and then using "cs:" was also a
	       poor choice.  */
	    && !result_cs.hasMatch () || result_sat.capturedStart (1) != result_cs.capturedStart (1))
	{
		sat = result_sat.captured (1).toInt ();
	}
	if (result_gamma.hasMatch ())
		gamma = result_gamma.captured (1).toInt ();
	if (result_brite.hasMatch ())
		brightness = result_brite.captured (1).toInt ();
	if (result_rot.hasMatch ())
		rot = result_rot.captured (1).toInt ();
	mirrored = result_mir.hasMatch ();
	if (result_wb.hasMatch ()) {
		white.setRed (result_wb.captured (1).toInt ());
		white.setGreen (result_wb.captured (2).toInt ());
		white.setBlue (result_wb.captured (3).toInt ());
	}
	if (result_cs.hasMatch ())
		cspace_idx = result_cs.captured (1).toInt ();
	if (result_cs.hasMatch () && result_sat.hasMatch ()
	    && result_sat.capturedStart (1) != result_cs.capturedStart (1) && sat == cspace_idx) {
		printf ("oops\n");
	}
	s.replace (re_b, "");
	s.replace (re_wb, "");
	s.replace (re_sat, "");
	s.replace (re_gamma, "");
	s.replace (re_brite, "");
	s.replace (re_rot, "");
	s.replace (re_mir, "");
	s.replace (re_cs, "");
	unknown_tags = s;
#if 0
	if (!s.isEmpty ())
		qDebug () << "unknown tags: " << s;
#endif
	return false;
}

void MainWindow::start_threads ()
{
	m_render_thread = new QThread;
	m_render_thread->start ();
	m_renderer = new Renderer;
	m_renderer->moveToThread (m_render_thread);
	connect (m_render_thread, &QThread::finished, m_renderer, &QObject::deleteLater);
	connect (m_renderer, &Renderer::signal_render_complete, this, &MainWindow::slot_render_complete);
	connect (this, &MainWindow::signal_render, m_renderer, &Renderer::slot_render);
}

// Called only when the render thread is idle.
void MainWindow::prune_lru (int leave)
{
	dir_entry **pnext = &m_lru;
	int count = 0;
	while (*pnext && count < leave) {
		count++;
		pnext = &(*pnext)->lru_next;
	}
	while (*pnext) {
		dir_entry *e = *pnext;
		e->lru_remove ();
		/* Discard images.  */
		e->images = nullptr;
		// printf ("Discarded %d: %s\n", (int)(e - &m_model.vec[0]), e->name.toStdString ().c_str ());
	}
	m_queue.erase (std::remove_if (m_queue.begin (), m_queue.end (),
				       [&] (auto &elt) -> bool
				       {
					       dir_entry &e = m_model.vec[elt.idx];
					       return e.lru_pprev == nullptr && elt.idx != m_idx;
				       }),
		    m_queue.end ());
}

void MainWindow::image_wheel_event (QWheelEvent *e)
{
	if (e->angleDelta().y() == 0)
		return;

	bool_changer bc (m_inhibit_updates, true);
	ui->scaleComboBox->setCurrentIndex (0);
	if (e->angleDelta().y() < 0)
		m_free_scale /= 1.1;
	else
		m_free_scale *= 1.1;
	scale_changed (0);
}

void MainWindow::pick_wb (QMouseEvent *e)
{
	auto pos = e->pos ();
	auto scene_pos = ui->imageView->mapToScene (pos);
	int px = scene_pos.x ();
	int py = scene_pos.y ();

	QSize sz = m_img->pixmap ().size ();
	if (px >= 0 && px < sz.width () && py >= 0 && py < sz.height ()) {
		auto &entry = m_model.vec[m_idx];
		QImage img = entry.images->on_disk.toImage ();
		QRgb v = img.pixel (px * m_img_scale, py * m_img_scale);
		v &= 0xFFFFFF;
		QColor clinear1 = srgb_to_linear (QColor (v));
		int cmax = std::max ({ clinear1.red (), clinear1.green (), clinear1.blue () });
		double factor = 255.0 / cmax;
		QColor clinear = QColor (clinear1.red () * factor, clinear1.green () * factor, clinear1.blue () * factor);
		entry.tweaks.white = clinear;
		update_wbcol_button (entry.tweaks.white);
		update_adjustments ();
	}
}

void MainWindow::image_mouse_event (QMouseEvent *e)
{
	if (m_img == nullptr)
		return;

	if (ui->wbPickButton->isChecked ())
		return pick_wb (e);

	if (e->type () == QEvent::MouseButtonRelease) {
		m_mouse_moving = false;
		return;
	}
	if (e->type () == QEvent::MouseMove && m_mouse_moving) {
		int xoff = e->x () - m_mouse_xbase;
		int yoff = e->y () - m_mouse_ybase;
		m_mouse_xbase = e->x ();
		m_mouse_ybase = e->y ();
		QScrollBar *hsb = ui->imageView->horizontalScrollBar ();
		QScrollBar *vsb = ui->imageView->verticalScrollBar ();
		hsb->setValue (hsb->value () - xoff);
		vsb->setValue (vsb->value () - yoff);
		return;
	}
	if (e->type () != QEvent::MouseButtonPress)
		return;

	m_mouse_moving = true;
	m_mouse_xbase = e->x ();
	m_mouse_ybase = e->y ();
}

void MainWindow::slot_save_as (bool)
{
	QSettings settings;
	QString ipath = settings.value ("paths/images").toString ();
	QFileDialog dlg (this, tr ("Save image file"), ipath, "PNG (*.png)");
	int filesel = settings.value ("filesel").toInt ();
	if (filesel == 0)
		dlg.setOption (QFileDialog::DontUseNativeDialog);
	dlg.setAcceptMode (QFileDialog::AcceptSave);
	dlg.setDefaultSuffix (".png");
	if (!dlg.exec ()) {
		return;
	}
#if 0
	QStringList flist = dlg.selectedFiles ();
	if (flist.isEmpty ()) {
		m_paused = false;
		restart_computation ();
		return;
	}
	QString filename = flist[0];

	QImage img = current_fd ().julia ? m_img_julia : m_img_mandel;
	if (!img.save (filename, "png"))
		QMessageBox::warning (this, PACKAGE, tr ("Failed to save image!"));
#endif
}

void MainWindow::help_about ()
{
	QString txt = "<p>" PACKAGE "</p>";
	txt = tr ("<p>Copyright \u00a9 2022\nBernd Schmidt &lt;bernds_cb1@t-online.de&gt;</p>");
	txt += tr ("<p>This is a simple image viewer that allows for some simple nondestructive editing.</p>"
		   "<p>Space and 'b' go forward and backward in the list of images.</p>"
		   "<p>Use 'f' and 't' to show or hide the files and tools panes.</p>"
		   "<p>Use 'z' to toggle scaling.</p>"
		   "<p>Press 'F5' to start a randomized slideshow.</p>"
		   "<o>Using Ctrl-'c' and Ctrl-'v', or the "
		   "corresponding buttons in the tools pane, you can copy and paste edit settings such as "
		   "white balance.</p><p>See the README for more information.</p>");

	QMessageBox mb (this);
	mb.setWindowTitle (PACKAGE);
	mb.setTextFormat (Qt::RichText);
	mb.setText (txt);
	mb.exec ();
}

/* Prepare for a change of the item model by flushing the render thread and
   increasing the generation number.  */
void MainWindow::update_model_gen ()
{
	/* First, wait for the render thread to complete its current job.  We then flush
	   the queue so that any call to slot_render_complete just exits.  */
	m_renderer->completion_sem.acquire ();
	m_renderer->completion_sem.release ();
	m_model_gen++;
}

void MainWindow::discard_entries ()
{
	update_model_gen ();

	bool_changer bc (m_inhibit_updates, true);

	m_idx = -1;
	ui->action_Rename->setEnabled (false);
	ui->action_Delete->setEnabled (false);
	m_next_slide = -1;
	delete m_img;
	m_img = nullptr;
	m_slide_timer.stop ();
	m_resize_timer.stop ();
	m_sliding = false;
	m_lru = nullptr;
	m_queue.clear ();
	m_model.reset ();
}

int MainWindow::scan_cwd (QString prev_entry_name)
{
	int new_idx = -1;
	auto dirlist = m_cwd.entryInfoList (QDir::Dirs | QDir::NoDot, QDir::Name);
	auto filelist = m_cwd.entryInfoList (QDir::Files, QDir::Name);
	for (auto &dn: dirlist)
		m_model.vec.emplace_back (m_cwd, dn.fileName (), true);
	m_first_file_idx = m_model.vec.size ();
	for (auto &fn: filelist) {
		QString s = fn.fileName ();
		m_model.vec.emplace_back (m_cwd, s, false);
		if (!prev_entry_name.isEmpty () && s == prev_entry_name)
			new_idx = m_model.vec.size () - 1;
	}
	return new_idx;
}

/* Used in only one place: to scan arguments given on the command line.  */
void MainWindow::scan (const QString &filename)
{
	bool opened = false;
	if (!filename.isEmpty ()) {
		QFileInfo f (filename);

		if (f.exists () && !f.isDir ()) {
			opened = true;
			m_cwd = f.dir ();
			m_model.vec.emplace_back (f.dir (), f.fileName (), false);
			m_first_file_idx = 0;
		}
	}
	if (!opened) {
		m_cwd = QDir (filename);
		if (!m_cwd.exists ()) {
			fprintf (stderr, "unknown file or directory\n");
			exit (1);
		}
		scan_cwd ();
	}
}

#if 0
/* Abandoned for the moment - Import image settings for the current directory, by analogy with picasa.ini.  */
void MainWindow::import_settings ()
{
	QString path = m_cwd.absoluteFilePath (DB_FILENAME);
	if (!QFile::exists (path))
		return;
	m_img_settings = std::make_unique<QSettings> (path, QSettings::IniFormat);
	for (auto &e: m_model.vec) {
		if (e.isdir)
			continue;
		if (m_img_settings->contains (e.name))
		{
		}
	}
}
#endif

/* Take an image from the queue and signal the render thread.
   This must ensure that if the current image is queued, it is picked first.  */

void MainWindow::restart_render ()
{
	// Reentering this function is always a mistake, so ensure that it doesn't happen.
	static bool reentry_guard = false;
	if (reentry_guard)
		abort ();
	bool_changer bc (reentry_guard, true);

	Renderer *r = m_renderer;

	/* Move the current image to the back.  */
	auto it = m_queue.begin ();
	auto end = m_queue.end ();
	while (it != end) {
		if (it->idx == m_idx) {
			std::rotate (it, it + 1, end);
			assert (m_queue.back ().idx == m_idx);
			break;
		}
		it++;
	}

	for (;;) {
		if (m_render_queued || m_queue.empty ())
			return;

		imgq q = m_queue.back ();
		m_queue.pop_back ();
		auto &entry = m_model.vec[q.idx];
		if (q.load) {
			// printf ("Loading %d\n", q.idx);
			load (q.idx, false);
		}
		img *img = entry.images.get ();
		if (img == nullptr || img->on_disk.isNull ())
			continue;
		if (q.changed) {
			img->corrected = QPixmap ();
			img->scaled = QPixmap ();
		}
		if (r->completion_sem.available () == 0)
			abort ();
		// printf ("queue render %d, wait (%d)\n", q.idx, r->completion_sem.available ());
		r->completion_sem.acquire ();
		// printf ("queue render %d\n", q.idx);
		m_render_queued = true;
		bool tweaked = ui->tweaksGroupBox->isChecked ();
		img_tweaks *tw = tweaked ? &entry.tweaks : &m_no_tweaks;
		QSize sz = size_for_image (entry, false);
		emit signal_render (q.idx, m_model_gen, entry.images.get (), tw, sz.width (), sz.height (),
				    tweaked);
		break;
	}
}

void MainWindow::slot_render_complete (int idx, int gen)
{
	if (gen != m_model_gen)
		return;

	// printf ("render complete: %d\n", idx);
	m_render_queued = false;
	prune_lru ();
	if (idx == m_idx)
		rescale_current ();
	restart_render ();
}

void MainWindow::enqueue_render (int idx, bool changed, bool load)
{
	for (auto &q: m_queue)
		if (q.idx == idx) {
			q.changed |= changed;
			q.load |= load;
			return;
		}
	m_queue.emplace_back (idx, changed, load);
	restart_render ();
}

void MainWindow::load_adjustments (dir_entry &e)
{
	QSqlQuery q (m_db);
	QString qstr = QString ("select tweaks from img_tweaks where md5=\'%1\'").arg (e.hash);
	// qDebug () << qstr;
	if (q.exec (qstr) && q.next ()) {
		// qDebug () << "good " << q.value (0);
		if (e.tweaks.from_string (q.value (0).toString ()))
			send_tweaks_to_db (e);
	}
}

QString MainWindow::load (int idx, bool do_queue)
{
	auto &entry = m_model.vec[idx];
	if (!entry.images)
		entry.images = std::make_unique<img> ();
	QString path = entry.path ();
	QFileInfo info (path);
	if (entry.images->on_disk.isNull () || entry.images->mtime != info.lastModified ())
	{
		if (!entry.images->on_disk.isNull ()) {
			/* Files were modified.  Flush the render thread, then remove old images.  */
			m_renderer->completion_sem.acquire ();
			m_renderer->completion_sem.release ();
			entry.images->linear = QImage ();
			entry.images->corrected = QPixmap ();
			entry.images->scaled = QPixmap ();
			/* We'll load new adjustments, if any, later.  */
			entry.tweaks = m_no_tweaks;
		}
		QPixmap pm;
		if (!pm.load (path)) {
			entry.hash = QString ();
			return QString ();
		}
		int w = pm.width ();
		int h = pm.height ();
		if (w > 2 && h > 2) {
			QImage img = pm.toImage ();
			uint64_t ravg = 0;
			uint64_t gavg = 0;
			uint64_t bavg = 0;
			for (int x = 1; x < w - 1; x++) {
				QColor c1 = img.pixel (x, 0);
				QColor c2 = img.pixel (x, h - 1);
				ravg += c1.red () + c2.red ();
				gavg += c1.green () + c2.green ();
				bavg += c1.blue () + c2.blue ();
			}
			entry.images->border_avgh = ravg * l_factor_r + gavg * l_factor_g + bavg * l_factor_b;
			entry.images->border_avgh /= 2 * (w - 2);
			entry.images->border_avgh /= 255;
			ravg = gavg = bavg = 0;
			for (int y = 0; y < h; y++) {
				QColor c1 = img.pixel (0, y);
				QColor c2 = img.pixel (w - 1, y);
				ravg += c1.red () + c2.red ();
				gavg += c1.green () + c2.green ();
				bavg += c1.blue () + c2.blue ();
			}
			entry.images->border_avgv = ravg * l_factor_r + gavg * l_factor_g + bavg * l_factor_b;
			entry.images->border_avgv /= 2 * h;
			entry.images->border_avgv /= 255;
		}
		QFile f (path);
		f.open (QIODevice::ReadOnly);
		// MD5 is apparently quite bad, but it is supposed to be used to
		// identify thumbnail files. If we ever want to support thumbnails,
		// it makes little sense to compute two different hashes for the same file.
		// So, stick with MD5. It's not like correct white balance is likely to
		// be security relevant.
		QCryptographicHash hash (QCryptographicHash::Md5);
		hash.addData (&f);
		f.close ();
		entry.hash = hash.result ().toBase64 (QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
		entry.images->mtime = info.lastModified ();
		entry.images->on_disk = pm;
		load_adjustments (entry);
		if (do_queue)
			enqueue_render (idx);
	}
	m_cur_img_size = info.size ();
	return entry.name;
}

void MainWindow::update_background ()
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	img *img = entry.images.get ();
	if (img == nullptr || img->on_disk.isNull ())
		return;
	QSize viewsz = ui->imageView->viewport ()->size ();
	QSize imgsz = img->on_disk.size ();
	// vx / vy > ix / iy: image is taller, use side entries.
	double v = imgsz.height () * viewsz.width () > imgsz.width () * viewsz.height () ? img->border_avgv : img->border_avgh;
	QSettings settings;
	int style = 0;
	if (settings.contains ("mainwin/background"))
		style = settings.value ("mainwin/background").toInt ();
	char chex[10] = "";
	if (style == 5) {
		sprintf (chex, "#%02x%02x%02x", (int)(255 * v), (int)(255 * v), (int)(255 * v));
	}

	// You'd think this should be using linear RGB somehow, but this seems to work better
	// than anything else I tried.
	QColor c (style == 5 ? QColor (chex)
		  : style == 0 ? QColor (Qt::black)
		  : style == 1 ? QColor ("#404040")
		  : style == 2 ? QColor ("#808080")
		  : style == 3 ? QColor ("#c0c0c0")
		  : QColor (Qt::white));
	ui->imageView->setBackgroundBrush (c);
}

QSize MainWindow::size_for_image (const dir_entry &entry, bool shown)
{
	int scale_idx = ui->scaleComboBox->currentIndex ();
	bool do_scale = scale_idx > 0 || m_free_scale != 1;

	img *img = entry.images.get ();
	if (img->on_disk.isNull ())
		return QSize ();

	update_background ();

	QSize img_sz = img->on_disk.size ();
	if (entry.tweaks.rot == 90 || entry.tweaks.rot == 270)
		img_sz.transpose ();

	QSize wanted_sz = img_sz;

	if (scale_idx == 0) {
		wanted_sz *= m_free_scale;
		if (shown)
			m_img_scale = m_free_scale;
	} else {
		QSize sz = ui->imageView->viewport ()->size ();
		QSize scale_sz = sz;
		if (scale_idx == 1)
			scale_sz.setHeight (INT_MAX);
		wanted_sz.scale (scale_sz, Qt::KeepAspectRatio);
		if (shown) {
			m_img_scale = wanted_sz.width () == 0 ? 1 : (double)img_sz.width () / wanted_sz.width ();
		}
	}

	return wanted_sz;
}

void MainWindow::rescale_current ()
{
	if (m_idx == -1)
		return;

	int scale_idx = ui->scaleComboBox->currentIndex ();
	bool do_scale = scale_idx > 0 || m_free_scale != 1;

	auto &entry = m_model.vec[m_idx];
	img *img = entry.images.get ();
	if (img->on_disk.isNull ())
		return;

	update_background ();

	Renderer *r = m_renderer;
	r->mutex.lock ();
	/* See if the renderer has completed a usable image. This is verified a bit more
	   a little further down.  */
	QPixmap preferred = do_scale ? img->scaled : img->corrected;
	r->mutex.unlock ();

	// line_terminator lt (stdout);

	QSize wanted_sz = size_for_image (entry, true);

	bool preferred_good = false;
	if (!preferred.isNull ()) {
		QSize existing_sz = preferred.size ();
		// printf ("found preferred %d x %d", existing_sz.width (), existing_sz.height ());
		if (entry.tweaks.rot == entry.images->render_rot
		    && entry.tweaks.mirrored == entry.images->render_mirror
		    && (!ui->tweaksGroupBox->isChecked ()
			|| entry.tweaks.cspace_idx == entry.images->linear_cspace_idx)
		    && ui->tweaksGroupBox->isChecked () == entry.images->render_tweaks
		    && (!do_scale || wanted_sz == existing_sz))
		{
			preferred_good = true;
			// printf ("good picture, no work needed ");
		}
		if (!preferred_good) {
			// printf ("enqueue again ");
			enqueue_render (m_idx);
		}

	}
	QPixmap final_img = preferred;
	if (!preferred_good) {
		final_img = img->on_disk;
		if (entry.tweaks.rot != 0 || entry.tweaks.mirrored) {
			QTransform t;
			t.rotate (entry.tweaks.rot);
			if (entry.tweaks.mirrored)
				t.scale(-1, 1);
			final_img = final_img.transformed (t);
		}
		if (do_scale)
			final_img = final_img.scaled (wanted_sz, Qt::KeepAspectRatio, Qt::FastTransformation);
	}
	if (m_img && m_img->pixmap ().toImage () == final_img.toImage ()) {
		// printf ("image good already ");
	} else {
		delete m_img;
		m_img = new QGraphicsPixmapItem (final_img);
		m_canvas.addItem (m_img);
	}
	m_canvas.setSceneRect (m_canvas.itemsBoundingRect ());
	QSize imgsz = final_img.size ();
	QSize sz = ui->imageView->viewport ()->size ();
	QSize newsz = sz - imgsz;
	// printf ("%d %d %d %d -> %d %d\n", sz.width (), sz.height (), imgsz.width (), imgsz.height (), newsz.width (), newsz.height ());
#if 1
	if (do_scale) {
		QPoint p (0, 0);
		QRect sr (p, imgsz);
		m_img->setPos (p);
		m_canvas.setSceneRect (sr);
	}
#else
	m_img->setScenePos (newsz.width () / 2, newsz.height () / 2);
#endif
}

void MainWindow::scale_changed (int idx)
{
	Qt::ScrollBarPolicy hpol = idx > 1 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
	Qt::ScrollBarPolicy vpol = idx > 0 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
	if (idx == 0 && m_free_scale > 1)
		hpol = vpol = Qt::ScrollBarAsNeeded;
	ui->imageView->setVerticalScrollBarPolicy (vpol);
	ui->imageView->setHorizontalScrollBarPolicy (hpol);
	{
		bool_changer bc (m_inhibit_updates, true);
		ui->scaleComboBox->setCurrentIndex (idx);
	}
	rescale_current ();
}

void MainWindow::rotate (int r)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	img *img = entry.images.get ();
	if (img == nullptr || img->on_disk.isNull ())
		return;

	entry.tweaks.rot += r;
	if (entry.tweaks.rot < 0)
		entry.tweaks.rot += 360;
	else if (entry.tweaks.rot >= 360)
		entry.tweaks.rot -= 360;

	send_tweaks_to_db (entry);
	rescale_current ();
}

void MainWindow::mirror (bool vertical)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	img *img = entry.images.get ();
	if (img == nullptr || img->on_disk.isNull ())
		return;

	entry.tweaks.mirrored = !entry.tweaks.mirrored;
	if (vertical)
		entry.tweaks.rot += 180;
	if (entry.tweaks.rot >= 360)
		entry.tweaks.rot -= 360;

	send_tweaks_to_db (entry);
	rescale_current ();
}

void MainWindow::update_wbcol_button (QColor col)
{
	QPixmap p (32, 32);
	p.fill (linear_to_srgb (col));
	QIcon i (p);
	ui->wbColButton->setIcon (i);
}

/* Values were changed: Ensure UI elements correspond to the current values.  */
void MainWindow::update_tweaks_ui (const dir_entry &entry)
{
	bool_changer bc (m_inhibit_updates, true);
	ui->blackSlider->setValue (entry.tweaks.blacklevel);
	ui->brightSlider->setValue (entry.tweaks.brightness);
	ui->gammaSlider->setValue (entry.tweaks.gamma);
	ui->satSlider->setValue (entry.tweaks.sat);
	ui->cspaceComboBox->setCurrentIndex (entry.tweaks.cspace_idx);
	update_wbcol_button (entry.tweaks.white);
}

void MainWindow::clear_lru ()
{
	while (m_lru != nullptr) {
		auto *p = m_lru;
		m_lru = p->lru_next;
		p->lru_pprev = nullptr;
		p->lru_next = nullptr;
	}
}

void MainWindow::add_to_lru (dir_entry &entry)
{
	if (entry.lru_pprev != nullptr)
		return;
	if (m_lru)
		m_lru->lru_pprev = &entry.lru_next;
	entry.lru_next = m_lru;
	entry.lru_pprev = &m_lru;
	m_lru = &entry;
}

bool MainWindow::switch_to (int idx)
{
	if (m_idx == idx)
		return true;

	/* The current image must not be on the LRU list.  */
	if (m_idx != -1) {
		auto &old_entry = m_model.vec[m_idx];
		assert (old_entry.lru_pprev == nullptr);
		add_to_lru (old_entry);
	}

	{
		bool_changer bc (m_inhibit_updates, true);
		QItemSelectionModel *sel = ui->fileView->selectionModel ();
		auto mmidx = m_model.index (idx);
		sel->select (mmidx, QItemSelectionModel::Current | QItemSelectionModel::ClearAndSelect);
		ui->fileView->scrollTo (mmidx);
	}

	m_idx = idx;
	ui->action_Rename->setEnabled (idx != -1);
	ui->action_Delete->setEnabled (idx != -1);

	auto &entry = m_model.vec[m_idx];
	entry.lru_remove ();
	QString n = load (idx);
	if (n.isEmpty ()) {
		ui->sizeLabel->setText ("");
		return false;
	}
	double v = m_cur_img_size;
	double kib = v / 1024;
	double mib = kib / 1024;
	if (v < 10000)
		ui->sizeLabel->setText (QString::number (m_cur_img_size));
	else if (kib < 10000)
		ui->sizeLabel->setText (QString::number (kib, 'f', 1) + " KiB");
	else if (mib < 10000)
			ui->sizeLabel->setText (QString::number (mib, 'f', 1) + " MiB");
	else
		ui->sizeLabel->setText (QString::number (mib / 1024, 'f', 1) + " GiB");

	update_tweaks_ui (entry);
	setWindowTitle (QString (PACKAGE) + " (experiment): " + n);
	rescale_current ();

	QScrollBar *hsb = ui->imageView->horizontalScrollBar ();
	QScrollBar *vsb = ui->imageView->verticalScrollBar ();
	hsb->setValue (0);
	vsb->setValue (0);
	return true;
}

void MainWindow::update_selection ()
{
	if (m_inhibit_updates)
		return;

	QItemSelectionModel *sel = ui->fileView->selectionModel ();
	const QModelIndexList &selected = sel->selectedRows ();
	if (selected.length () == 1) {
		QModelIndex i = selected.first ();
		switch_to (i.row ());
	}
}

void MainWindow::files_doubleclick ()
{
	QItemSelectionModel *sel = ui->fileView->selectionModel ();
	const QModelIndexList &selected = sel->selectedRows ();
	if (selected.length () == 1) {
		QModelIndex i = selected.first ();
		int idx = i.row ();
		if (idx < 0 || idx >= m_model.vec.size ())
			return;
		if (!m_model.vec[idx].isdir)
			return;
		QDir newd = m_cwd;
		if (!newd.cd (m_model.vec[idx].name) || !newd.exists () || !newd.isReadable ())
			return;
		discard_entries ();
		m_cwd = newd;
		scan_cwd ();
	}
}

void MainWindow::slot_rescan (bool)
{
	QString cur;
	if (m_idx != -1) {
		auto &entry = m_model.vec[m_idx];
		if (!entry.isdir) {
			cur = entry.name;
		}
	}
	discard_entries ();
	int idx = scan_cwd (cur);
	if (idx != -1)
		switch_to (idx);
}

void MainWindow::slot_rename (bool)
{
	QString cur;
	if (m_idx == -1)
		/* Shouldn't happen.  */
		return;
	auto &entry = m_model.vec[m_idx];
	RenameDialog dlg (this, &entry);
	if (dlg.exec ()) {
		if (!m_individual_files)
			slot_rescan ();
	}
}

void MainWindow::slot_delete (bool)
{
	QString cur;
	if (m_idx == -1)
		/* Shouldn't happen.  */
		return;
	auto &entry = m_model.vec[m_idx];
	QMessageBox mb (QMessageBox::Question, tr ("Really delete file"),
			tr ("Do you really want to delete \"%1\" in directory \"%2\"?").arg (entry.name).arg(entry.dir.absolutePath ()),
			QMessageBox::Yes | QMessageBox::No);
	if (mb.exec () != QMessageBox::Yes)
		return;

	if (entry.dir.remove (entry.name)) {
		update_model_gen ();
		clear_lru ();
		int idx = m_idx;
		{
			bool_changer bc (m_inhibit_updates, true);
			m_model.removeRows (idx, 1);
		}
		m_idx = -1;
		update_selection ();
	} else
		QMessageBox::warning (this, PACKAGE, tr ("The delete operation failed."));
}

void MainWindow::next_image (bool)
{
	if (m_idx == -1)
		return;

	int next = m_idx;
	while (next + 1 < m_model.vec.size ()) {
		next++;
		if (switch_to (next))
			break;
	}
	if (next + 1 < m_model.vec.size ()) {
		next++;
		add_to_lru (m_model.vec[next]);
		enqueue_render (next, false, true);
	}
}

void MainWindow::prev_image (bool)
{
	if (m_idx == -1)
		return;

	int prev = m_idx;
	while (prev > 0) {
		prev--;
		if (m_model.vec[prev].isdir)
			return;
		if (switch_to (prev))
			break;
	}
	if (prev > 0) {
		prev--;
		if (m_model.vec[prev].isdir)
			return;
		add_to_lru (m_model.vec[prev]);
		enqueue_render (prev, false, true);
	}
}

void MainWindow::slide_elapsed ()
{
	if (!m_sliding)
		return;

	if (m_next_slide != -1)
		switch_to (m_next_slide);

	int count = m_model.vec.size () - m_first_file_idx;
	if (count > 1) {
		for (int i = 0; i < 20; i++) {
			int new_idx = m_first_file_idx + rand () % count;
			if (!load (new_idx).isEmpty ()) {
				enqueue_render (new_idx);
				restart_render ();
				m_next_slide = new_idx;
				break;
			}
		}
	}

	m_slide_timer.start (ui->slideTimeSpinBox->value ());
}

void MainWindow::start_slideshow (bool)
{
	setWindowState (Qt::WindowFullScreen);
	m_sliding = true;
	srand (time (nullptr));
	slide_elapsed ();
}

void MainWindow::stop (bool)
{
	m_sliding = false;
	setWindowState (Qt::WindowNoState);
	m_slide_timer.stop ();
}

void MainWindow::perform_resizes ()
{
	rescale_current ();
}

void MainWindow::sync_to_db ()
{
	if (m_db_queue.isEmpty ())
		return;
	m_db.transaction ();

	// qDebug () << "transaction:";
	QSqlQuery q (m_db);
	while (!m_db_queue.isEmpty ()) {
		QString s = m_db_queue.takeFirst ();
		// qDebug () << "  " + s;
		q.exec (s);
	}
	m_db.commit ();
}

void MainWindow::send_tweaks_to_db (const dir_entry &entry)
{
	QString str = entry.tweaks.to_string ();
	QSqlQuery q (m_db);
	QString qstr;
	if (str.isNull ())
		qstr = QString ("delete from img_tweaks where md5 = '%1'").arg (entry.hash);
	else
		qstr = QString ("replace into img_tweaks(md5, tweaks) values('%1', '%2')").arg (entry.hash, str);
	if (m_db_queue.isEmpty ())
		m_db_timer.start ();
	m_db_queue << qstr;
}

void MainWindow::update_adjustments ()
{
	if (m_inhibit_updates || m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;

	entry.tweaks.brightness = ui->brightSlider->value ();
	entry.tweaks.blacklevel = ui->blackSlider->value ();
	entry.tweaks.gamma = ui->gammaSlider->value ();
	entry.tweaks.sat = ui->satSlider->value ();
	entry.tweaks.cspace_idx = ui->cspaceComboBox->currentIndex ();

	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::do_autoblack (bool)
{
	if (m_idx == -1)
		return;

	Renderer *r = m_renderer;
	/* Ensure the current job is complete so that we have the necessary data.  */
	r->completion_sem.acquire ();
	r->completion_sem.release ();

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	// printf ("min %d %d %d %d\n", entry.images->l_minr, entry.images->l_ming, entry.images->l_minb, entry.images->l_minavg);
#if 0
	ui->blackSlider->setValue (std::min ({ entry.images->l_minr, entry.images->l_ming, entry.images->l_minb }) / 256);
#else
	ui->blackSlider->setValue (entry.images->l_minavg / 256);
#endif
}

void MainWindow::do_copy (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	m_copied_tweaks = entry.tweaks;
}

void MainWindow::do_paste (const img_tweaks &source)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	bool changed = false;
	if (ui->pasteBriteCheckBox->isChecked ()) {
		changed |= entry.tweaks.brightness != source.brightness;
		entry.tweaks.brightness = source.brightness;
	}
	if (ui->pasteBCheckBox->isChecked ()) {
		changed |= entry.tweaks.blacklevel != source.blacklevel;
		entry.tweaks.blacklevel = source.blacklevel;
	}
	if (ui->pasteGCheckBox->isChecked ()) {
		changed |= entry.tweaks.gamma != source.gamma;
		entry.tweaks.gamma = source.gamma;
	}
	if (ui->pasteSCheckBox->isChecked ()) {
		changed |= entry.tweaks.sat != source.sat;
		entry.tweaks.sat = source.sat;
	}
	if (ui->pasteCSCheckBox->isChecked ()) {
		changed |= entry.tweaks.cspace_idx != source.cspace_idx;
		entry.tweaks.cspace_idx = source.cspace_idx;
	}
	if (ui->pasteWBCheckBox->isChecked ()) {
		changed |= entry.tweaks.white != source.white;
		entry.tweaks.white = source.white;
	}
	update_tweaks_ui (entry);
	if (changed) {
		send_tweaks_to_db (entry);
		enqueue_render (m_idx, true);
	}
}

void MainWindow::clear_wb (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	if (entry.tweaks.white == Qt::white)
		return;
	update_wbcol_button (Qt::white);
	entry.tweaks.white = Qt::white;
	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::choose_wb_color (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;

	QColor oldc = entry.tweaks.white;
	QColorDialog dlg (linear_to_srgb (oldc), this);
	dlg.setCurrentColor (linear_to_srgb (oldc));
	dlg.setWindowTitle (tr ("Choose white balance color"));
	// dlg.setOption (QColorDialog::DontUseNativeDialog);
	connect (&dlg, &QColorDialog::currentColorChanged,
		 [this, &entry] (QColor c)
		 {
			 entry.tweaks.white = srgb_to_linear (c);
			 enqueue_render (m_idx, true);
			 restart_render ();
		 });
	if (!dlg.exec ()) {
		entry.tweaks.white = oldc;
		enqueue_render (m_idx, true);
		restart_render ();
	} else {
		update_wbcol_button (entry.tweaks.white);
		send_tweaks_to_db (entry);
	}
}

void MainWindow::clear_black (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	if (entry.tweaks.blacklevel == 0)
		return;
	{
		bool_changer bc (m_inhibit_updates, true);
		entry.tweaks.blacklevel = 0;
		ui->blackSlider->setValue (0);
	}
	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::clear_brightness (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	if (entry.tweaks.brightness == 0)
		return;
	{
		bool_changer bc (m_inhibit_updates, true);
		entry.tweaks.brightness = 0;
		ui->brightSlider->setValue (0);
	}
	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::clear_gamma (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	if (entry.tweaks.gamma == 0)
		return;
	{
		bool_changer bc (m_inhibit_updates, true);
		entry.tweaks.gamma = 0;
		ui->gammaSlider->setValue (0);
	}
	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::clear_sat (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	if (entry.tweaks.sat == 0)
		return;
	{
		bool_changer bc (m_inhibit_updates, true);
		entry.tweaks.sat = 0;
		ui->satSlider->setValue (0);
	}
	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::clear_cspace (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	if (entry.tweaks.cspace_idx == 0)
		return;
	{
		bool_changer bc (m_inhibit_updates, true);
		entry.tweaks.cspace_idx = 0;
		ui->cspaceComboBox->setCurrentIndex (0);
	}
	send_tweaks_to_db (entry);
	enqueue_render (m_idx, true);
}

void MainWindow::keyPressEvent (QKeyEvent *e)
{
	switch (e->key ())
	{
		case Qt::Key_Alt:
			menuBar ()->show ();
			break;

		default:
			break;
	}
	QMainWindow::keyPressEvent (e);
}

void MainWindow::keyReleaseEvent (QKeyEvent *e)
{
	switch (e->key ())
	{
		case Qt::Key_Alt:
			if (!ui->action_ShowMenubar->isChecked ())
				menuBar ()->hide ();
			break;

		default:
			break;
	}
	QMainWindow::keyPressEvent (e);
}

void MainWindow::restore_geometry ()
{
	QSettings settings;
	restoreGeometry (settings.value("mainwin/geometry").toByteArray());
	restoreState (settings.value("mainwin/windowState").toByteArray());
	if (settings.contains("mainwin/scaleState"))
		ui->scaleComboBox->setCurrentIndex (settings.value("mainwin/scaleState").toBool () ? 2 : 0);
	if (settings.contains("mainwin/showmenu"))
		ui->action_ShowMenubar->setChecked (settings.value("mainwin/showmenu").toBool ());
}

void MainWindow::closeEvent (QCloseEvent *event)
{
	setWindowState (Qt::WindowNoState);
	m_slide_timer.stop ();
	m_resize_timer.stop ();
	m_db_timer.stop ();
	sync_to_db ();

	QSettings settings;
	settings.setValue ("mainwin/geometry", saveGeometry ());
	settings.setValue ("mainwin/windowState", saveState ());
	settings.setValue ("mainwin/showmenu", ui->action_ShowMenubar->isChecked ());
	settings.setValue ("mainwin/scaleState", ui->scaleComboBox->currentIndex () != 0);

	QMainWindow::closeEvent (event);
}

void MainWindow::prefs ()
{
	PrefsDialog dlg (this);
	if (dlg.exec ()) {
		update_background ();
	}
}

/* Don't do this in the constructor to avoid threads going active before it's finished.  */
void MainWindow::perform_setup ()
{
	m_resize_timer.setSingleShot (true);
	connect (&m_resize_timer, &QTimer::timeout, this, &MainWindow::perform_resizes);

	for (int i = 0; i < m_model.vec.size (); i++)
		if (!m_model.vec[i].isdir) {
			switch_to (i);
			break;
		}
}

MainWindow::MainWindow (const QStringList &files)
	: ui (new Ui::MainWindow), m_db (QSqlDatabase::database (PACKAGE))
{
	ui->setupUi (this);
	ui->imageView->setScene (&m_canvas);

	ui->action_ShowMenubar->setChecked (true);
	ui->action_Rename->setEnabled (false);
	ui->action_Delete->setEnabled (false);

	restore_geometry ();

	menuBar ()->setVisible (ui->action_ShowMenubar->isChecked ());
	statusBar ()->hide ();
	start_threads ();

	if (files.size () <= 1) {
		QString f (files.empty () ? "." : files[0]);
		QFileInfo fi (f);
		if (fi.exists () && fi.isDir ())
			scan (f);
	}
	for (auto f: files) {
		QFileInfo fi (files[0]);
		if (fi.exists () && !fi.isDir ()) {
			m_individual_files = true;
			scan (f);
		}
	}

	// Not ready
	ui->linksWidget->hide ();

	ui->fileView->setModel (&m_model);
	connect (ui->fileView->selectionModel (), &QItemSelectionModel::selectionChanged,
		 [this] (const QItemSelection &, const QItemSelection &) { update_selection (); });
	connect (ui->fileView, &ClickableListView::doubleclicked, this, &MainWindow::files_doubleclick);

	m_setup_timer.setSingleShot (true);
	connect (&m_setup_timer, &QTimer::timeout, this, &MainWindow::perform_setup);
	m_setup_timer.start (10);

	m_slide_timer.setSingleShot (true);
	connect (&m_slide_timer, &QTimer::timeout, this, &MainWindow::slide_elapsed);

	m_db_timer.setInterval (3000);
	connect (&m_db_timer, &QTimer::timeout, this, &MainWindow::sync_to_db);

	connect (ui->imageView, &SizeGraphicsView::resized, [this] () { m_resize_timer.start (10); });

	connect (ui->imageView, &SizeGraphicsView::mouse_event, this, &MainWindow::image_mouse_event);
	connect (ui->imageView, &SizeGraphicsView::wheel_event, this, &MainWindow::image_wheel_event);

	ui->action_Rescan->setEnabled (!m_individual_files);
	connect (ui->action_Rescan, &QAction::triggered, this, &MainWindow::slot_rescan);
	connect (ui->action_Rename, &QAction::triggered, this, &MainWindow::slot_rename);
	connect (ui->action_Delete, &QAction::triggered, this, &MainWindow::slot_delete);

	connect (ui->action_Next, &QAction::triggered, this, &MainWindow::next_image);
	connect (ui->action_Prev, &QAction::triggered, this, &MainWindow::prev_image);
	connect (ui->action_Slideshow, &QAction::triggered, this, &MainWindow::start_slideshow);
	connect (ui->action_Stop, &QAction::triggered, this, &MainWindow::stop);

	connect (ui->action_ShowMenubar, &QAction::toggled, [&] (bool v) { menuBar ()->setVisible (v); });

	connect (ui->wbColButton, &QPushButton::clicked, this, &MainWindow::choose_wb_color);
	connect (ui->wbClearButton, &QPushButton::clicked, this, &MainWindow::clear_wb);
	connect (ui->blackClearButton, &QPushButton::clicked, this, &MainWindow::clear_black);
	connect (ui->gammaClearButton, &QPushButton::clicked, this, &MainWindow::clear_gamma);
	connect (ui->satClearButton, &QPushButton::clicked, this, &MainWindow::clear_sat);
	connect (ui->brightClearButton, &QPushButton::clicked, this, &MainWindow::clear_brightness);
	connect (ui->csClearButton, &QPushButton::clicked, this, &MainWindow::clear_cspace);
	connect (ui->blackAutoButton, &QPushButton::clicked, this, &MainWindow::do_autoblack);
	connect (ui->brightSlider, &QSlider::valueChanged, [this] (int) { update_adjustments (); });
	connect (ui->blackSlider, &QSlider::valueChanged, [this] (int) { update_adjustments (); });
	connect (ui->gammaSlider, &QSlider::valueChanged, [this] (int) { update_adjustments (); });
	connect (ui->satSlider, &QSlider::valueChanged, [this] (int) { update_adjustments (); });
	connect (ui->tweaksGroupBox, &QGroupBox::toggled, [this] (bool) { update_adjustments (); });
	void (QComboBox::*cic) (int) = &QComboBox::currentIndexChanged;
	connect (ui->cspaceComboBox, cic, [this] (int idx) { update_adjustments (); });

	connect (ui->action_Copy, &QAction::triggered, this, &MainWindow::do_copy);
	connect (ui->copyButton, &QPushButton::clicked, this, &MainWindow::do_copy);
	connect (ui->action_Paste, &QAction::triggered, [this] (bool) { do_paste (m_copied_tweaks); });
	connect (ui->pasteButton, &QPushButton::clicked, [this] (bool) { do_paste (m_copied_tweaks); });
	connect (ui->clearAllButton, &QPushButton::clicked, [this] (bool) { do_paste (m_no_tweaks); });

	connect (ui->action_Preferences, &QAction::triggered, this, &MainWindow::prefs);
	connect (ui->action_Quit, &QAction::triggered, this, &MainWindow::close);

	connect (ui->action_About, &QAction::triggered, this, &MainWindow::help_about);
	connect (ui->action_AboutQt, &QAction::triggered, [this] (bool) { QMessageBox::aboutQt (this); });

	connect (ui->action_RCW, &QAction::triggered, [this] (bool) { rotate (90); });
	connect (ui->action_RCCW, &QAction::triggered, [this] (bool) { rotate (-90); });
	connect (ui->action_MH, &QAction::triggered, [this] (bool) { mirror (false); });
	connect (ui->action_FV, &QAction::triggered, [this] (bool) { mirror (true); });

	connect (ui->action_Scale, &QAction::triggered,
		 [this] (bool)
		 {
			 int idx = ui->scaleComboBox->currentIndex ();
			 idx = (idx + 1) % 3;
			 m_free_scale = 1;
			 ui->scaleComboBox->setCurrentIndex (idx);
		 });
	connect (ui->action_DSize, &QAction::triggered,
		 [this] (bool)
		 {
			 m_free_scale *= 2;
			 scale_changed (0);
		 });
	connect (ui->action_HSize, &QAction::triggered,
		 [this] (bool)
		 {
			 m_free_scale /= 2;
			 scale_changed (0);
		 });
	connect (ui->action_ZReset, &QAction::triggered,
		 [this] (bool)
		 {
			 m_free_scale = 1;
			 scale_changed (0);
		 });
	connect (ui->scaleComboBox, cic, this, &MainWindow::scale_changed);

	QAction *view_first = ui->menu_View->actions().at(0);
	QAction *fa = ui->filesDock->toggleViewAction ();
	QAction *ta = ui->toolsDock->toggleViewAction ();
	ui->menu_View->insertAction (view_first, fa);
	ui->menu_View->insertAction (view_first, ta);
	fa->setShortcut(Qt::Key_F);
	ta->setShortcut(Qt::Key_T);

	addActions ({ ui->action_Quit, ui->action_Rename, ui->action_Delete, ui->action_Rescan });
	addActions ({ fa, ta });
	addActions ({ ui->action_ShowMenubar });
	addActions ({ ui->action_Scale, ui->action_ZReset });
	addActions ({ ui->action_Copy, ui->action_Paste });
	addActions ({ ui->action_RCW, ui->action_RCCW, ui->action_MH, ui->action_FV, ui->action_DSize, ui->action_HSize });
	addActions ({ ui->action_Next, ui->action_Prev, ui->action_Slideshow, ui->action_Stop });
}

MainWindow::~MainWindow ()
{
	delete ui;
}

int main (int argc, char **argv)
{
	QApplication::setAttribute (Qt::AA_EnableHighDpiScaling);
	QApplication myapp (argc, argv);

	myapp.setOrganizationName ("bernds");
	myapp.setApplicationName (PACKAGE);

	QCommandLineParser cmdp;

	cmdp.addHelpOption ();
	cmdp.addPositionalArgument ("path", QObject::tr ("Oepn <path> as a file or directory."));

	cmdp.process (myapp);

        QStringList imgdirs = QStandardPaths::standardLocations (QStandardPaths::PicturesLocation);
	if (imgdirs.isEmpty ()) {
		fprintf (stderr, "error: could not find standard Pictures directory for storing the database");
		exit (1);
	}
	QSqlDatabase::addDatabase ("QSQLITE", PACKAGE);
	auto db = QSqlDatabase::database (PACKAGE);
	db.setDatabaseName (QDir (imgdirs[0]).filePath (DB_FILENAME));
	if (db.open ()) {
		// printf ("db open success\n");
		QSqlQuery create ("create table if not exists img_tweaks (md5 string primary key, tweaks string)", db);
		QSqlQuery sync (db);
		sync.exec ("pragma synchronous=off");
	}
	QSettings settings;

	const QStringList args = cmdp.positionalArguments ();
	auto w = new MainWindow (args);
	w->show ();
	auto retval = myapp.exec ();
	return retval;
}
