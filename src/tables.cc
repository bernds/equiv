/*
 *   tables.cpp = part of mainwindow
 */
#include "imgentry.h"

void simple_fs_model::reset ()
{
	beginResetModel ();
	vec.clear ();
	endResetModel ();
}

QVariant simple_fs_model::data (const QModelIndex &index, int role) const
{
	int row = index.row ();
	int col = index.column ();

	if (row < 0)
		return QVariant ();

	size_t r = row;
	if (r >= vec.size ())
		return QVariant ();

	const dir_entry &e = vec[r];
	if (role == Qt::TextAlignmentRole)
		return Qt::AlignLeft;
	if (role != Qt::DisplayRole)
		return QVariant ();
	return e.name;
}

const dir_entry *simple_fs_model::find (const QModelIndex &index) const
{
	int row = index.row ();
	if (row < 0)
		return nullptr;

	size_t r = row;
	if (r >= vec.size ())
		return nullptr;
	return &vec[r];
}

bool simple_fs_model::removeRows (int row, int count, const QModelIndex &parent)
{
	if (row < 0 || count < 1 || (size_t) (row + count) > vec.size ())
		return false;
	beginRemoveRows (parent, row, row + count - 1);
	vec.erase (vec.begin () + row, vec.begin () + (row + count));
	endRemoveRows ();
	return true;
}

QModelIndex simple_fs_model::index (int row, int col, const QModelIndex &) const
{
	return createIndex (row, col);
}

QModelIndex simple_fs_model::parent (const QModelIndex &) const
{
	return QModelIndex ();
}

int simple_fs_model::rowCount (const QModelIndex &) const
{
	return vec.size ();
}

int simple_fs_model::columnCount (const QModelIndex &) const
{
	return 1;
}
