#pragma once
#include <QObject>
#include <QThread>

#include <QVector>
#include <QImage>

#include <vector>

#include <opencv2/opencv.hpp>

class MontageLabelMatchWorker:
    public QThread
{
    Q_OBJECT
private:
    std::vector<cv::Mat> images;
    cv::Mat label;
public:
    void run() override;
    MontageLabelMatchWorker(const QVector<QImage>& images, const QVector<QImage>& labels);

    struct Result
    {
        QString msg;
        QImage label;
        QImage image;
    };
signals:
    void resultReady(const Result& result);
};