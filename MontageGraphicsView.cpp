#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QDragMoveEvent>
#include <QtEndian>

#include "MontageGraphicsView.h"

void MontageGraphicsView::adjustCam()
{
	qreal cx = backGround->x() + backGround->pixmap().width() / 2;
	qreal cy = backGround->y() + backGround->pixmap().height() / 2;
	crossLine[0]->setPos(cx, cy);
	crossLine[1]->setPos(cx, cy);
	this->centerOn(cx, cy);
	this->update();
}

MontageGraphicsView::MontageGraphicsView(QWidget* parent)
	:QGraphicsView(parent)
{
	// here the order of creation determines the order of render
	backGround = scene.addPixmap(QPixmap());
	foreGround = scene.addPixmap(QPixmap());
	crossLine[0] = scene.addLine(-10.0, 0, 10.0, 0, QPen(Qt::GlobalColor::black));
	crossLine[1] = scene.addLine(0, -10.0, 0, 10.0, QPen(Qt::GlobalColor::red));
	this->setScene(&scene);
}

void MontageGraphicsView::loadBackgroudImage(const QImage& img)
{
	backGround->setPixmap(QPixmap::fromImage(img));
	adjustCam();
}

void MontageGraphicsView::clearBackgroundImage()
{
	backGround->setPixmap(QPixmap()); // clear
}

void MontageGraphicsView::loadForegroundImage(const QImage& img, const QColor& col)
{
	// convert black pixiels to transparent
	QImage alphaImg = img.convertToFormat(QImage::Format::Format_ARGB32);
	union rgba
	{
		uint rgba32;
		uchar rgba8[4];
	};
	rgba* bits = (rgba*)alphaImg.bits();
	static rgba testBLEndian = { 0xffff0000 };
	
	int len = alphaImg.width() * alphaImg.height();
	const QVector<QColor>& imgCols = *srcImgLblCols;
	if (testBLEndian.rgba8[0] == 0xff) // big endian
		while (len > 0)
		{
			if (bits->rgba32 == 0xff000000)
				bits->rgba8[0] = 0;
			else
			{
				bits->rgba8[0] = 255;
				bits->rgba8[1] = col.red();
				bits->rgba8[2] = col.green();
				bits->rgba8[3] = col.blue();
			}
			bits++;
			len--;
		}
	else // little endian
		while (len > 0)
		{
			if (bits->rgba32 == 0xff000000)
				bits->rgba8[3] = 0;
			else
			{
				bits->rgba8[3] = 255;
				bits->rgba8[2] = col.red();
				bits->rgba8[1] = col.green();
				bits->rgba8[0] = col.blue();
			}
			bits++;
			len--;
		}

	foreGround->setPixmap(QPixmap::fromImage(alphaImg));
}

void MontageGraphicsView::clearForegroundImage()
{
	foreGround->setPixmap(QPixmap()); // clear
}

void MontageGraphicsView::dragMoveEvent(QDragMoveEvent* event)
{
	
}