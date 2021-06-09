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

void MontageGraphicsView::loadForegroundImage(const QImage& img)
{
	foreGround->setPixmap(QPixmap::fromImage(img));
}

void MontageGraphicsView::clearForegroundImage()
{
	foreGround->setPixmap(QPixmap()); // clear
}

void MontageGraphicsView::dragMoveEvent(QDragMoveEvent* event)
{
	
}