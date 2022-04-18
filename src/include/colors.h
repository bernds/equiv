#ifndef COLORS_H
#define COLORS_H

#include <QColor>
#include <cstdint>
#include <cmath>

static inline double srgb_to_linear (int v)
{
	double s = v / 255.;
	if (s < 0.04045)
		return s / 12.92;
	constexpr double a = 0.055;
	return pow ((s + a) / (1 + a), 2.4);
}

static inline int linear_to_srgb (double v)
{
	if (v < 0.0031308)
		return floor (v * 12.92 * 255);
	constexpr double a = 0.055;
	double nv = (1 + a) * pow (v, 1 / 2.4) - a;
	return floor (nv * 255);
}

static inline QColor srgb_to_linear (QColor corig)
{
	return QColor (255 * srgb_to_linear (corig.red ()),
		       255 * srgb_to_linear (corig.green ()),
		       255 * srgb_to_linear (corig.blue ()));
}

static inline QColor linear_to_srgb (QColor corig)
{
	return QColor (linear_to_srgb (corig.red () / 255.),
		       linear_to_srgb (corig.green () / 255.),
		       linear_to_srgb (corig.blue () / 255.));
}

#endif
