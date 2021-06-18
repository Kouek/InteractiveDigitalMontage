#pragma once
#include <QObject>
#include <QThread>
#include <QVector>
#include <QImage>

#include <vector>

#include <opencv2/opencv.hpp>

#include "MontageCore.h"

class MontageLabelMatchResult
{
public:
    QString msg;
    QImage expndLbl;
    QImage image;
    QImage colLbl;
};

class MontageLabelMatchWorker:
    public QThread
{
    Q_OBJECT
private:
    std::vector<cv::Mat> images;
    cv::Mat label;
    std::vector<cv::Vec3b> imageColors;
    double largePenalty;
    double smoothAlpha;
    MontageCore::SmoothTermType smoothType;
    // The colored label buffered for current Labeling process.
    // We need this since designatedLbls may change during Labeling.
    QImage colLabel;
public:
    void run() override;
    MontageLabelMatchWorker(
        const QVector<QImage>& images,
        const QVector<QImage>& labels,
        const QVector<QColor>& imagesColors,
        double largePenalty, double smoothAlpha, int smoothType
    );

signals:
    void resultReady(const MontageLabelMatchResult& result);
};