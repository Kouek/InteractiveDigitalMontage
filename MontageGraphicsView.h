#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QVector>

class MontageGraphicsView :
    public QGraphicsView
{
    Q_OBJECT
private:
    QGraphicsScene scene;
    QGraphicsLineItem* crossLine[2] = { nullptr, nullptr };
    QGraphicsPixmapItem* backGround = nullptr;
    QGraphicsPixmapItem* foreGround = nullptr;
private:
    void adjustCam();
public:
    MontageGraphicsView(QWidget* parent = nullptr);
public:
    void loadBackgroudImage(const QImage& img);
    void clearBackgroundImage();
    void loadForegroundImage(const QImage& img);
    void clearForegroundImage();
protected:
    void dragMoveEvent(QDragMoveEvent* event) override;
};