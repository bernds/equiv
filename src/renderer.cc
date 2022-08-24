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

void Renderer::slot_render (int idx, int gen, img *e, img_tweaks *tw, int w, int h, bool tweaked)
{
	// printf ("start render %d: %d x %d rot %d (was %d)\n", idx, w, h, tw->rot, e->render_rot);
	mutex.lock ();
	QPixmap pm = e->on_disk;
	QImage linear = e->linear;
	QPixmap corrected = e->corrected;
	mutex.unlock ();

	if (!abort_render) {
		if (linear.isNull () || e->linear_cspace_idx != tw->cspace_idx) {
			linear = pm.toImage ().convertToFormat (QImage::Format_RGBA64);
			if (tw->cspace_idx != 0)
				linear.setColorSpace ((QColorSpace::NamedColorSpace)tw->cspace_idx);
			else if (!linear.colorSpace ().isValid ())
				linear.setColorSpace (QColorSpace::SRgb);
			QColorSpace linear_cs = linear.colorSpace ();
			linear_cs.setTransferFunction (QColorSpace::TransferFunction::Linear);
			linear.convertToColorSpace (linear_cs);
			auto bits1 = linear.bits ();
			uint64_t *bits = (uint64_t *)bits1;
			QSize sz = linear.size ();
			long count = (long)sz.width () * (long)sz.height ();
			int maxr = 1, maxg = 1, maxb = 1;
			int minr = 65535, ming = 65535, minb = 65535, mina = 65535;
			for (long i = 0; i < count; i++) {
				uint64_t v = *bits;
				int b = v & 65535;
				v >>= 16;
				int g = v & 65535;
				v >>= 16;
				int r = v & 65535;

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

		double rlimit = 65535. / (e->l_maxr * fr);
		double glimit = 65535. / (e->l_maxg * fg);
		double blimit = 65535. / (e->l_maxb * fb);
		double limit = std::min ({ 1.0, rlimit, glimit, blimit });
		if (corrected.isNull () || e->render_tweaks != tweaked || e->render_rot != tw->rot) {
			QImage tmp = linear;
			double gammaval = 1 + tw->gamma / 100.1;
			double satval = -tw->sat / 100.;
			double bright = 1 + tw->brightness / 100.;

			if (tw->blacklevel != 0 || tw->brightness != 0 || tw->sat != 0 || tw->gamma != 0 || tw->white != Qt::white) {
				auto bits1 = tmp.bits ();
				uint64_t *bits = (uint64_t *)bits1;
				QSize sz = tmp.size ();
				long count = (long)sz.width () * (long)sz.height ();
				uint64_t black = tw->blacklevel * 256;
				float scale = bright * 65536. / (65536. - black);
				// printf ("black %d max %d %d %d scales: %f %f %f limit: %f\n", (int)black, e->l_maxr, e->l_maxg, e->l_maxb, fr, fg, fb, limit);
				scale *= limit;
				black *= scale;
				for (long i = 0; i < count; i++) {
					uint64_t v = *bits;
					int r = v & 65535;
					v >>= 16;
					int g = v & 65535;
					v >>= 16;
					int b = v & 65535;
					v >>= 16;
					r = std::clamp ((int)(r * fr * scale - black), 0, 65535);
					g = std::clamp ((int)(g * fg * scale - black), 0, 65535);
					b = std::clamp ((int)(b * fb * scale - black), 0, 65535);

					if (satval != 0) {
						int lumi = r * l_factor_r + g * l_factor_g + b * l_factor_b;
						r = std::clamp ((int)(r + satval * (lumi - r)), 0, 65535);
						g = std::clamp ((int)(g + satval * (lumi - g)), 0, 65535);
						b = std::clamp ((int)(b + satval * (lumi - b)), 0, 65535);
					}
					if (gammaval != 1) {
						r = pow (r / 65535., gammaval) * 65535;
						g = pow (g / 65535., gammaval) * 65535;
						b = pow (b / 65535., gammaval) * 65535;
					}
					v <<= 32;
					v |= b << 16;
					v |= g;
					v <<= 16;
					v |= r;
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
			if (tw->rot != 0) {
				QTransform t;
				t.rotate (tw->rot);
				tmp = tmp.transformed (t);
			}
			corrected = QPixmap::fromImage (tmp.convertToFormat (QImage::Format_ARGB32));
		}
		QPixmap scaled = corrected.scaled (w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		QMutexLocker lock (&mutex);
		e->linear = linear;
		e->corrected = corrected;
		e->scaled = scaled;
		e->render_rot = tw->rot;
		e->linear_cspace_idx = tw->cspace_idx;
		e->render_tweaks = tweaked;
	}
	completion_sem.release ();
	// printf ("end render %d\n", idx);
	emit signal_render_complete (idx, gen);
}

