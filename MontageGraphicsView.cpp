#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QMouseEvent>
#include <QtEndian>

#include <QDebug>

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

void MontageGraphicsView::configIntrctPath(bool enable, int w, const QColor& col)
{
	if (enable == false)
	{
		intrctPathDat.enable = false;
		return;
	}
	intrctPathDat.enable = true;
	intrctPathDat.width = w;

	// recreate IntrctPath
	if (intrctPath != nullptr)
		scene.removeItem(intrctPath);
	QPen pen;
	pen.setColor(col);
	pen.setWidth(w);
	intrctPath = scene.addPath(
		intrctPathDat.path,
		pen
	);
	intrctPath->setZValue(
		(foreGround->zValue() + crossLine[0]->zValue()) / 2
	);

	// create or config intrctCursor
	if (intrctCursor == nullptr)
	{
		intrctCursor = scene.addEllipse(0, 0, w, w, QPen(Qt::GlobalColor::red));
		intrctCursor->setZValue(
			(intrctPath->zValue() + crossLine[0]->zValue()) / 2
		);
	}
	else
		intrctCursor->setRect(0, 0, w, w);
}

void MontageGraphicsView::clearIntrctPath()
{
	if (intrctPathDat.enable == false)return;
	intrctPathDat.path.clear();
	if (intrctPath != nullptr)
		intrctPath->setPath(intrctPathDat.path);
}

void MontageGraphicsView::mousePressEvent(QMouseEvent* event)
{
	if (intrctPathDat.enable == false)return;

	QPointF targetPos = mapToScene(event->pos());

	if (!backGround->boundingRect().contains(targetPos))return;
	intrctPathDat.path.moveTo(targetPos);
	intrctPathDat.isPressing = true;
}

void MontageGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
	if (intrctPathDat.enable == false)return;

	QPointF targetPos = mapToScene(event->pos());

	intrctCursor->setPos(
		targetPos.x() - 0.5 * intrctPathDat.width,
		targetPos.y() - 0.5 * intrctPathDat.width
	);

	if (intrctPathDat.isPressing == false)return;
	if (!backGround->boundingRect().contains(targetPos))return;
	if ((intrctPathDat.path.currentPosition() - targetPos).manhattanLength() < intrctPathDat.width)
		return;
	intrctPathDat.path.lineTo(targetPos);
	intrctPath->setPath(intrctPathDat.path);
}

void MontageGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
	if (intrctPathDat.enable == false)return;
	intrctPathDat.isPressing = false;
}
