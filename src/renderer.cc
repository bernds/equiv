#include <cmath>
#include <cfloat>
#include <algorithm>

#include <QVector>
#include <QSemaphore>
#include <QColorSpace>

#include "mainwindow.h"
#include "colors.h"

static inline uint32_t color_merge (uint32_t c1, uint32_t c2, double m1)
{
	double m2 = 1 - m1;
	int r = ((c1 >> 16) & 255) * m1 + ((c2 >> 16) & 255) * m2;
	int g = ((c1 >> 8) & 255) * m1 + ((c2 >> 8) & 255) * m2;
	int b = ((c1 >> 0) & 255) * m1 + ((c2 >> 0) & 255) * m2;
	return (r << 16) | (g << 8) | b;
}

class runner : public QRunnable
{
	QSemaphore *completion_sem;
	std::atomic<bool> *success, *abort_render;
	int w, y0, y0e;
	QRgb *data;

public:
	runner (QSemaphore *sem, std::atomic<bool> *succ_in, std::atomic<bool> *abrt,
		int w_in, int y0_in, int y0e_in, QRgb *data_in)
		: completion_sem (sem), success (succ_in), abort_render (abrt),
		  w (w_in), y0 (y0_in), y0e (y0e_in), data (data_in)
	{
		setAutoDelete (true);
	}

	void run () override
	{
		for (int y = y0; y < y0e; y++) {
			if (abort_render->load ()) {
				break;
			}
			for (int x = 0; x < w; x++) {
			}
		}
		completion_sem->release ();
	}
};

void Renderer::do_render ()
{
}

void Renderer::slot_render (int idx, img *e, img_tweaks *tw, int w, int h)
{
	// printf ("start render %d\n", idx);
	mutex.lock ();
	QPixmap pm = e->on_disk;
	QImage linear = e->linear;
	QPixmap corrected = e->corrected;
	mutex.unlock ();

	if (!abort_render) {
		if (linear.isNull ()) {
			linear = pm.toImage ().convertToFormat (QImage::Format_RGB30);
			if (!linear.colorSpace ().isValid ())
				linear.setColorSpace (QColorSpace::SRgb);
			linear.convertToColorSpace (QColorSpace::SRgbLinear);
			auto bits1 = linear.bits ();
			uint32_t *bits = (uint32_t *)bits1;
			QSize sz = linear.size ();
			long count = (long)sz.width () * (long)sz.height ();
			int32_t maxr = 1, maxg = 1, maxb = 1;
			int32_t minr = 1023, ming = 1023, minb = 1023, mina = 1023;
			for (long i = 0; i < count; i++) {
				uint32_t v = *bits;
				int b = v & 0x3ff;
				v >>= 10;
				int g = v & 0x3ff;
				v >>= 10;
				int r = v & 0x3ff;

				maxr = std::max (r, maxr);
				maxg = std::max (g, maxg);
				maxb = std::max (b, maxb);
				minr = std::min (r, minr);
				ming = std::min (g, ming);
				minb = std::min (b, minb);
				int avg = (r + b + g) / 3;
				mina = std::min (avg, mina);
				bits++;
			}
			// printf ("count %ld, max %d %d %d\n", count, maxr, maxg, maxb);
			e->l_maxr = maxr;
			e->l_maxg = maxg;
			e->l_maxb = maxb;
			e->l_minr = minr;
			e->l_ming = ming;
			e->l_minb = minb;
			e->l_minavg = mina;
		}

		int wr = std::max (1, tw->white.red ());
		int wg = std::max (1, tw->white.green ());
		int wb = std::max (1, tw->white.blue ());
		int wmax = std::max ({wr, wg, wb});
		double fr = (double)wmax / wr;
		double fg = (double)wmax / wg;
		double fb = (double)wmax / wb;

		double rlimit = 1023. / (e->l_maxr * fr);
		double glimit = 1023. / (e->l_maxg * fg);
		double blimit = 1023. / (e->l_maxb * fb);
		double limit = std::min ({ 1.0, rlimit, glimit, blimit });
		if (corrected.isNull ()) {
			QImage tmp = linear;
			double gammaval = 1 + tw->gamma / 100.1;
			if (tw->blacklevel != 0 || tw->gamma != 0 || tw->white != Qt::white) {
				auto bits1 = tmp.bits ();
				uint32_t *bits = (uint32_t *)bits1;
				QSize sz = tmp.size ();
				long count = (long)sz.width () * (long)sz.height ();
				uint32_t black = tw->blacklevel * 4;
				float scale = 1024. / (1024. - black);
				// printf ("black %d max %d %d %d scales: %f %f %f limit: %f\n", (int)black, e->l_maxr, e->l_maxg, e->l_maxb, fr, fg, fb, limit);
				scale *= limit;
				black *= scale;
				for (long i = 0; i < count; i++) {
					uint32_t v = *bits;
					int32_t b = v & 0x3ff;
					v >>= 10;
					int32_t g = v & 0x3ff;
					v >>= 10;
					int32_t r = v & 0x3ff;

					r = std::clamp ((int32_t)(r * fr * scale - black), 0, 1023);
					g = std::clamp ((int32_t)(g * fg * scale - black), 0, 1023);
					b = std::clamp ((int32_t)(b * fb * scale - black), 0, 1023);

					if (gammaval != 1) {
						r = pow (r / 1023., gammaval) * 1023;
						g = pow (g / 1023., gammaval) * 1023;
						b = pow (b / 1023., gammaval) * 1023;
					}
					v = r << 10;
					v |= g;
					v <<= 10;
					v |= b;
					*bits++ = v;
				}
			}
#if 0 /* Doesn't seem to work??? */
			if (tw->gamma != 0) {
				QColorSpace gammacs = linear.colorSpace ().withTransferFunction (QColorSpace::TransferFunction::Gamma,
												 1 + tw->gamma / 100.1);
				tmp.convertToColorSpace (gammacs);
//				tmp.setColorSpace (QColorSpace::SRgbLinear);
			}
#endif
			tmp.convertToColorSpace (QColorSpace::SRgb);
			corrected = QPixmap::fromImage (tmp.convertToFormat (QImage::Format_ARGB32));
		}
		QPixmap scaled = corrected.scaled (w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		QMutexLocker lock (&mutex);
		e->linear = linear;
		e->corrected = corrected;
		e->scaled = scaled;
	}
	completion_sem.release ();
	// printf ("end render %d\n", idx);
	emit signal_render_complete (idx);
}

