#pragma once

#include <QtWidgets/QMainWindow>
#include <QLocale>

#include <QGraphicsScene>
#include <QVector>
#include <QImage>

#include "ui_InteractiveDigitalMontage.h"

#include "MontageThreadController.h"

class InteractiveDigitalMontage : public QMainWindow
{
    Q_OBJECT
private:
    enum class MainState
    {
        Initialized,
        SourceImageLoaded,
        Labeling,
        Labeled,
        GradientFusing,
        GradientFused
    };
    MainState state = MainState::Initialized;
    
    QVector<QImage> srcImgs;
    QVector<QString> srcImgNames;
    QVector<QColor> srcImgLblCols;
    QVector<QImage> intrctLbls;
    int imgW = 0, imgH = 0;
    
    int currSrcIdx = -1;
    void changeCurrSrcIdxTo(int newIdx); 

    QImage LMRslts[3];
    int currLMRsltIdx = 0;
public:
    void goToPreviousImage();
    void goToNextImage();
    void appendSourceImages();
    void clearSourceImages();

    void appendInteractiveLabels();
    void clearInteractiveLabels();
    void clearCurrentLabel();

    void runLabelMatching();
    void handleLblMatchRslt(const MontageLabelMatchResult& result);
    void switchLblMatchRslts();
public:
    InteractiveDigitalMontage(QWidget *parent = Q_NULLPTR);
private:
    Ui::InteractiveDigitalMontageClass ui;
};
