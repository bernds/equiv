#ifndef IMGENTRY_H
#define IMGENTRY_H 1

#include <QPixmap>
#include <QDateTime>
#include <QAbstractItemModel>

#include <memory>

struct img
{
	QDateTime mtime;
	QPixmap on_disk;
	QImage linear {};
	int l_maxr = 0, l_maxg = 0, l_maxb = 0;
	int l_minr = 0, l_ming = 0, l_minb = 0, l_minavg = 0;
	QPixmap corrected {};
	QPixmap scaled {};
	double border_avgh = 0;
	double border_avgv = 0;

	/* Information about what was applied to the corrected/scaled images.
	   Used to decide if they are up-to-date or need to be rerendered.  */
	int render_rot = 0;
	bool render_tweaks = false;
	int linear_cspace_idx = 0;

	~img ();
};

struct img_tweaks
{
	/* White balance.  */
	QColor white = Qt::white;
	/* An index into a combo box chosen to correspond to values of QColorSpace::NamedColorSpace.
	   Using an int instead of the enum to be able to signal 0 as default.  */
	int cspace_idx = 0;
	int blacklevel = 0;
	int brightness = 0;
	/* A range of -100 to 100 given by the GUI, translated into a reasonable 
	   gamma curve by the rendering code.  */
	int gamma = 0;
	int sat = 0;
	int rot = 0;

	QString unknown_tags;

	QString to_string () const;
	bool from_string (QString s);
};

struct dir_entry
{
	QString name;
	QString hash;
	std::unique_ptr<img> images;
	bool isdir = false;
	img_tweaks tweaks;

	dir_entry *lru_next = nullptr;
	dir_entry **lru_pprev = nullptr;

	void lru_remove ()
	{
		if (lru_next != nullptr)
			lru_next->lru_pprev = lru_pprev;
		if (lru_pprev != nullptr)
			*lru_pprev = lru_next;
		lru_next = nullptr;
		lru_pprev = nullptr;
	}
	dir_entry(QString n, bool dir) : name (std::move (n)), isdir (dir)
	{
	}
};

class simple_fs_model : public QAbstractItemModel
{
public:
	std::vector<dir_entry> vec;

	void reset ();
	const dir_entry *find (const QModelIndex &) const;
	virtual QVariant data (const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QModelIndex index (int row, int col = 0, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent (const QModelIndex &index ) const override;
	int rowCount (const QModelIndex &parent = QModelIndex()) const override;
	int columnCount (const QModelIndex &parent = QModelIndex()) const override;
	bool removeRows (int row, int count, const QModelIndex &parent = QModelIndex()) override;
};

#endif
