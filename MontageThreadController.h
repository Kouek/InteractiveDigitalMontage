#pragma once
#include <QObject>
#include <QThread>

#include <QVector>
#include <QImage>

#include <vector>

#include <opencv2/opencv.hpp>

class MontageLabelMatchResult
{
public:
    QString msg;
    QImage label;
    QImage image;
};

class MontageLabelMatchWorker:
    public QThread
{
    Q_OBJECT
private:
    std::vector<cv::Mat> images;
    cv::Mat label;
    std::vector<cv::Vec3b> imageColors;
public:
    void run() override;
    MontageLabelMatchWorker(
        const QVector<QImage>& images,
        const QVector<QImage>& labels,
        const QVector<QColor>& imagesColors
    );

signals:
    void resultReady(const MontageLabelMatchResult& result);
};