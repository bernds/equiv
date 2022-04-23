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

#include "colors.h"
#include "imgentry.h"
#include "util-widgets.h"

#include "prefsdlg.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#define PACKAGE "Equiv"
#define DB_FILENAME "equiv.db"

img::~img ()
{
}

QString img_tweaks::to_string () const
{
	return (QString::number (blacklevel)
		+ "," + QString::number (gamma)
		+ "," + QString::number (white.red ())
		+ "," + QString::number (white.green ())
		+ "," + QString::number (white.blue ()));
}

void img_tweaks::from_string (QString s)
{
	static QRegularExpression re ("(\\d+),(-?\\d+),(\\d+),(\\d+),(\\d+)");

	auto result = re.match (s);
	if (result.hasMatch ()) {
		blacklevel = result.captured (1).toInt ();
		gamma = result.captured (2).toInt ();
		white.setRed (result.captured (3).toInt ());
		white.setGreen (result.captured (4).toInt ());
		white.setBlue (result.captured (5).toInt ());
	}
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
#if 0
	if (e->angleDelta().y() == 0)
		return;
	if (e->angleDelta().y() < 0)
		zoom_out ();
	else
		zoom_in ();
#endif
}

void MainWindow::image_mouse_event (QMouseEvent *e)
{
	if (!ui->wbPickButton->isChecked () || m_img == nullptr)
		return;

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
		QColor corig (v);
		QColor clinear (255 * srgb_to_linear (corig.red ()), 255 * srgb_to_linear (corig.green ()), 255 * srgb_to_linear (corig.blue ()));
		entry.tweaks.white = clinear;
		update_wbcol_button (entry.tweaks.white);
		update_adjustments ();
	}
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

void MainWindow::discard_entries ()
{
	/* First, wait for the render thread to complete its current job.  We then flush
	   the queue so that any call to slot_render_complete just exits.  */
	m_renderer->completion_sem.acquire ();
	m_renderer->completion_sem.release ();

	bool_changer bc (m_inhibit_updates, true);

	m_idx = -1;
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

void MainWindow::scan_cwd ()
{
	auto dirlist = m_cwd.entryInfoList (QDir::Dirs | QDir::NoDot, QDir::Name);
	auto filelist = m_cwd.entryInfoList (QDir::Files, QDir::Name);
	for (auto &dn: dirlist)
		m_model.vec.emplace_back (dn.fileName (), true);
	m_first_file_idx = m_model.vec.size ();
	for (auto &fn: filelist)
		m_model.vec.emplace_back (fn.fileName (), false);
}

/* Used in only one place: to scan arguments given on the command line.  */
void MainWindow::scan (const QString &filename)
{
	QDir d (filename);
	bool opened = false;
	if (!filename.isEmpty ()) {
		QFileInfo f (filename);

		if (f.exists () && !f.isDir ()) {
			opened = true;
			m_cwd = f.dir ();
			m_model.vec.emplace_back (f.fileName (), false);
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
		QSize sz = ui->imageView->viewport ()->size ();
		if (r->completion_sem.available () == 0)
			abort ();
		// printf ("queue render %d, wait (%d)\n", q.idx, r->completion_sem.available ());
		r->completion_sem.acquire ();
		// printf ("queue render %d\n", q.idx);
		m_render_queued = true;
		img_tweaks *tw = ui->tweaksGroupBox->isChecked () ? &entry.tweaks : &m_no_tweaks;
		emit signal_render (q.idx, entry.images.get (), tw, sz.width (), sz.height ());
		break;
	}
}

void MainWindow::slot_render_complete (int idx)
{
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
		e.tweaks.from_string (q.value (0).toString ());
	}
}

QString MainWindow::load (int idx, bool do_queue)
{
	auto &entry = m_model.vec[idx];
	if (!entry.images)
		entry.images = std::make_unique<img> ();
	QString path = m_cwd.absoluteFilePath (entry.name);
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
			entry.images->border_avgh = ravg * 0.21 + gavg * 0.72 + bavg * 0.07;
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
			entry.images->border_avgv = ravg * 0.21 + gavg * 0.72 + bavg * 0.07;
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

void MainWindow::rescale_current ()
{
	if (m_idx == -1)
		return;

	bool do_scale = ui->scaleCheckBox->isChecked ();

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

	QSize img_sz = img->on_disk.size ();
	QSize wanted_sz = img_sz;
	QSize sz = ui->imageView->viewport ()->size ();
	wanted_sz.scale (sz, Qt::KeepAspectRatio);
	m_img_scale = wanted_sz.width () == 0 ? 1 : (double)img_sz.width () / wanted_sz.width ();

	bool preferred_good = false;
	if (!preferred.isNull ()) {
		// printf ("found preferred ");
		QSize existing_sz = preferred.size ();
		if (!do_scale || wanted_sz == existing_sz) {
			preferred_good = true;
			// printf ("good picture, no work needed ");
		} else {
			// printf ("enqueue again ");
			enqueue_render (m_idx);
		}

	}
	QPixmap final_img = preferred;
	if (!preferred_good) {
		final_img = img->on_disk;
		if (do_scale)
			final_img = img->on_disk.scaled (wanted_sz, Qt::KeepAspectRatio, Qt::FastTransformation);
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
	QSize newsz = sz;
	newsz -= imgsz;
	// printf ("%d %d %d %d -> %d %d", sz.width (), sz.height (), imgsz.width (), imgsz.height (), newsz.width (), newsz.height ());
#if 1
	if (do_scale) {
		QRect sr (QPoint (-newsz.width () / 2, -newsz.height () / 2), sz);
		m_canvas.setSceneRect (sr);
	}
#else
	m_img->setScenePos (newsz.width () / 2, newsz.height () / 2);
#endif
}

void MainWindow::scale_changed (int state)
{
	Qt::ScrollBarPolicy pol = state == Qt::Unchecked ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
	ui->imageView->setVerticalScrollBarPolicy (pol);
	ui->imageView->setHorizontalScrollBarPolicy (pol);
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
	ui->gammaSlider->setValue (entry.tweaks.gamma);
	update_wbcol_button (entry.tweaks.white);
}

bool MainWindow::switch_to (int idx)
{
	if (m_idx == idx)
		return true;

	/* The current image must not be on the LRU list.  */
	if (m_idx != -1) {
		auto &old_entry = m_model.vec[m_idx];
		assert (old_entry.lru_pprev == nullptr);
		if (m_lru)
			m_lru->lru_pprev = &old_entry.lru_next;
		old_entry.lru_next = m_lru;
		old_entry.lru_pprev = &m_lru;
		m_lru = &old_entry;
	}

	{
		bool_changer bc (m_inhibit_updates, true);
		QItemSelectionModel *sel = ui->fileView->selectionModel ();
		auto mmidx = m_model.index (idx);
		sel->select (mmidx, QItemSelectionModel::Current | QItemSelectionModel::ClearAndSelect);
		ui->fileView->scrollTo (mmidx);
	}

	m_idx = idx;
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
	setWindowTitle (QString (PACKAGE) + ": " + n);
	rescale_current ();
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

	QSqlQuery q (m_db);
	while (!m_db_queue.isEmpty ()) {
		QString s = m_db_queue.takeFirst ();
		q.exec (s);
	}
	m_db.commit ();
}

void MainWindow::send_tweaks_to_db (const dir_entry &entry)
{
	QString str = entry.tweaks.to_string ();
	QSqlQuery q (m_db);
	QString qstr = QString ("replace into img_tweaks(md5, tweaks) values(\'%1\', \'%2\')").arg (entry.hash, str);
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

	entry.tweaks.blacklevel = ui->blackSlider->value ();
	entry.tweaks.gamma = ui->gammaSlider->value ();

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

void MainWindow::do_paste (bool)
{
	if (m_idx == -1)
		return;

	auto &entry = m_model.vec[m_idx];
	if (entry.images.get () == nullptr || entry.images->on_disk.isNull ())
		return;
	bool changed = false;
	if (ui->pasteBCheckBox->isChecked ()) {
		changed |= entry.tweaks.blacklevel != m_copied_tweaks.blacklevel;
		entry.tweaks.blacklevel = m_copied_tweaks.blacklevel;
	}
	if (ui->pasteGCheckBox->isChecked ()) {
		changed |= entry.tweaks.gamma != m_copied_tweaks.gamma;
		entry.tweaks.gamma = m_copied_tweaks.gamma;
	}
	if (ui->pasteWBCheckBox->isChecked ()) {
		changed |= entry.tweaks.white != m_copied_tweaks.white;
		entry.tweaks.white = m_copied_tweaks.white;
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
		ui->scaleCheckBox->setChecked (settings.value("mainwin/scaleState").toBool ());
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
	settings.setValue ("mainwin/scaleState", ui->scaleCheckBox->isChecked ());

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
		if (fi.exists () && !fi.isDir ())
			scan (f);
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

	connect (ui->action_Next, &QAction::triggered, this, &MainWindow::next_image);
	connect (ui->action_Prev, &QAction::triggered, this, &MainWindow::prev_image);
	connect (ui->action_Slideshow, &QAction::triggered, this, &MainWindow::start_slideshow);
	connect (ui->action_Stop, &QAction::triggered, this, &MainWindow::stop);

	connect (ui->action_ShowMenubar, &QAction::toggled, [&] (bool v) { menuBar ()->setVisible (v); });

	connect (ui->wbColButton, &QPushButton::clicked, this, &MainWindow::choose_wb_color);
	connect (ui->wbClearButton, &QPushButton::clicked, this, &MainWindow::clear_wb);
	connect (ui->blackClearButton, &QPushButton::clicked, this, &MainWindow::clear_black);
	connect (ui->gammaClearButton, &QPushButton::clicked, this, &MainWindow::clear_gamma);
	connect (ui->blackAutoButton, &QPushButton::clicked, this, &MainWindow::do_autoblack);
	connect (ui->blackSlider, &QSlider::valueChanged, [this] (int) { update_adjustments (); });
	connect (ui->gammaSlider, &QSlider::valueChanged, [this] (int) { update_adjustments (); });
	connect (ui->tweaksGroupBox, &QGroupBox::toggled, [this] (bool) { update_adjustments (); });

	ui->copyButton->setDefaultAction (ui->action_Copy);
	ui->pasteButton->setDefaultAction (ui->action_Paste);
	connect (ui->action_Copy, &QAction::triggered, this, &MainWindow::do_copy);
	connect (ui->action_Paste, &QAction::triggered, this, &MainWindow::do_paste);

	connect (ui->action_Preferences, &QAction::triggered, this, &MainWindow::prefs);
	connect (ui->action_Quit, &QAction::triggered, this, &MainWindow::close);

	connect (ui->action_About, &QAction::triggered, this, &MainWindow::help_about);
	connect (ui->action_AboutQt, &QAction::triggered, [=] (bool) { QMessageBox::aboutQt (this); });

	connect (ui->action_Scale, &QAction::triggered, ui->scaleCheckBox, &QCheckBox::toggle);
	connect (ui->scaleCheckBox, &QCheckBox::stateChanged, this, &MainWindow::scale_changed);

	QAction *view_first = ui->menu_View->actions().at(0);
	QAction *fa = ui->filesDock->toggleViewAction ();
	QAction *ta = ui->toolsDock->toggleViewAction ();
	ui->menu_View->insertAction (view_first, fa);
	ui->menu_View->insertAction (view_first, ta);
	fa->setShortcut(Qt::Key_F);
	ta->setShortcut(Qt::Key_T);

	addActions ({ ui->action_Quit });
	addActions ({ fa, ta });
	addActions ({ ui->action_ShowMenubar });
	addActions ({ ui->action_Scale });
	addActions ({ ui->action_Next, ui->action_Prev, ui->action_Slideshow, ui->action_Stop });

#if 0
	void (QComboBox::*cic) (int) = &QComboBox::currentIndexChanged;
	void (QSpinBox::*changed) (int) = &QSpinBox::valueChanged;
	void (QDoubleSpinBox::*dchanged) (double) = &QDoubleSpinBox::valueChanged;

	addActions ({ ui->action_Mandelbrot, ui->action_Julia, ui->action_MJpreview });
	addActions ({ ui->action_IncPrec, ui->action_DecPrec });
	addActions ({ ui->action_FD2, ui->action_FD3, ui->action_FD4, ui->action_FD5 });
	addActions ({ ui->action_FD6, ui->action_FD7  });
	addActions ({ ui->action_AngleSmooth  });
	addActions ({ ui->action_GradEditor });
#endif
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
	}
	QSettings settings;

	const QStringList args = cmdp.positionalArguments ();
	auto w = new MainWindow (args);
	w->show ();
	auto retval = myapp.exec ();
	return retval;
}
