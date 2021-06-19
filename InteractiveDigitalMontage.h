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
    QVector<QImage> designatedLbls;
    int imgW = 0, imgH = 0;
    
    int currSrcIdx = -1;
    void changeCurrSrcIdxTo(int newIdx); 

    static const int LMRsltNum = 4;
    int currLMRsltIdx = 0;
    // In LMRslts:
    // 0 stores expanded colored label
    // 1 stores composite image
    // 2 stores 0 + 1
    // 3 stores 2 + colored label
    QImage LMRslts[LMRsltNum];

    QImage GFRslt;
public:
    void goToPreviousImage();
    void goToNextImage();
    void appendSourceImages();
    void clearSourceImages();

    void appendLoadedLabels();
    void appendInteractiveLabel();
    void eraseInteractiveLabel();
    void clearDesignatedLabels();
    void clearCurrentLabel();
    void updateStrokeWidth();

    void runLabelMatching();
    void handleLblMatchRslt(const MontageLabelMatchResult& result);
    void switchLblMatchRslts();
    void exportLblMatchRslt();

    void runGradientFuse();
    void handleGradFuseRslt(const MontageGradientFusionResult& result);
    void exportGradFuseRslt();

    void adjustSpinStepOnSmoothTypeChanged();
public:
    InteractiveDigitalMontage(QWidget *parent = Q_NULLPTR);
private:
    Ui::InteractiveDigitalMontageClass ui;
};
