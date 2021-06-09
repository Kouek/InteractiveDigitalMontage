#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QVector>

class MontageLabelPath
{
public:
    bool enable = false;
    bool isPressing = false;
    int width = 0;
    QPainterPath path;
};

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
    QGraphicsEllipseItem* intrctCursor = nullptr;
    QGraphicsPathItem* intrctPath = nullptr;
    MontageLabelPath intrctPathDat;
private:
    void adjustCam();
public:
    MontageGraphicsView(QWidget* parent = nullptr);
public:
    void loadBackgroudImage(const QImage& img);
    void clearBackgroundImage();
    void loadForegroundImage(const QImage& img);
    void clearForegroundImage();
public:
    void configIntrctPath(bool enable, int w = 0, const QColor& col = QColor());
    void clearIntrctPath();
    const QPainterPath& getIntrctPath() { return intrctPathDat.path; }
protected:
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
};