#include "InteractiveDigitalMontage.h"

#include <QDir>
#include <QFileDialog>

#include <QDebug>

// Usage:
//   Set text of <lnEdt> to be <txt> in color <col>
//   <lnEdt> should NOT be NULL
static void lineEditSetText(QLineEdit* lnEdt, const QString& txt, const QColor& col)
{
    static QString colStylSht =
        QString("color: %1");
    lnEdt->setText(txt);
    lnEdt->setStyleSheet(
        colStylSht.arg(col.name())
    );
}

// Usgae:
//   Set text of <txtEdt> in color <col>
//   If <isClear> is TRUE, set <txt> to <txtEdt>
//   Else append <txt> at the new line of <txtEdt>
//   <txtEdt> should NOT be NULL
static void textEditSetText(QTextEdit* txtEdt, const QString& txt, const QColor& bkCol, bool isClear)
{
    if (isClear)
        txtEdt->clear();
    txtEdt->append(txt);
    
    // set color of the line
    QTextCursor cursor = txtEdt->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    QTextCharFormat fmt;
    fmt.setBackground(bkCol);
    fmt.setForeground(
        QColor(
            255 - bkCol.red(),
            255 - bkCol.green(),
            255 - bkCol.blue()
        )
    );
    cursor.mergeCharFormat(fmt);
    cursor.clearSelection();
    cursor.movePosition(QTextCursor::EndOfLine);
}

// Usage:
//   Convert black pixels to transparent
//   Convert non-black pixels to color <col>
static QImage convertLabelColor(const QImage& label, const QColor& col)
{
    QImage alphaLbl = label.convertToFormat(QImage::Format::Format_ARGB32);
    union rgba
    {
        uint rgba32;
        uchar rgba8[4];
    };
    rgba* bits = (rgba*)alphaLbl.bits();
    static rgba testBLEndian = { 0xffff0000 };

    int len = alphaLbl.width() * alphaLbl.height();
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
    return alphaLbl;
}

// Usage:
//   Switch the current displayed source image and label concurrently
//   <newIdx> should follows: -1 <= <newIdx> < size_of_srcImgs
//   When <newIdx> is -1, clear all displayed source images and labels
void InteractiveDigitalMontage::changeCurrSrcIdxTo(int newIdx)
{
    static QString bkColStylSht =
        QString("background-color: %1");

    currSrcIdx = newIdx;
    if (!srcImgs.isEmpty() && currSrcIdx != -1)
    {
        ui.graphicsViewSrcImgs->loadBackgroudImage(
            srcImgs[currSrcIdx]
        );
        lineEditSetText(ui.lineEditSrcImgs, srcImgNames[currSrcIdx], Qt::GlobalColor::black);

        ui.labelSrcLblCol->setStyleSheet(
            bkColStylSht.arg(srcImgLblCols[currSrcIdx].name())
        );

        ui.graphicsViewIntrctLbls->loadBackgroudImage(
            srcImgs[currSrcIdx]
        );
        ui.graphicsViewIntrctLbls->clearForegroundImage();
        if (!designatedLbls[currSrcIdx].isNull())
            ui.graphicsViewIntrctLbls->loadForegroundImage(
                convertLabelColor(designatedLbls[currSrcIdx], srcImgLblCols[currSrcIdx])
            );

        ui.graphicsViewIntrctLbls->clearIntrctPath();
        ui.graphicsViewIntrctLbls->configIntrctPath(
            true,
            ui.verticalSliderStrokeWidth->value(),
            srcImgLblCols[currSrcIdx]
        );
        lineEditSetText(ui.lineEditIntrctLbls, srcImgNames[currSrcIdx], Qt::GlobalColor::black);
    }
    else
    {
        ui.graphicsViewSrcImgs->clearBackgroundImage();
        lineEditSetText(ui.lineEditSrcImgs, "", Qt::GlobalColor::black);

        ui.labelSrcLblCol->setStyleSheet(
            bkColStylSht.arg("white")
        );

        ui.graphicsViewIntrctLbls->clearBackgroundImage();
        ui.graphicsViewIntrctLbls->clearForegroundImage();
        lineEditSetText(ui.lineEditIntrctLbls, "", Qt::GlobalColor::black);
    }
}

void InteractiveDigitalMontage::goToPreviousImage()
{
    if (currSrcIdx == 0)
        changeCurrSrcIdxTo(srcImgs.size() - 1);
    else if (currSrcIdx > 0 && currSrcIdx <= srcImgs.size() - 1)
        changeCurrSrcIdxTo(currSrcIdx - 1);
}

void InteractiveDigitalMontage::goToNextImage()
{
    if (currSrcIdx == srcImgs.size() - 1)
        changeCurrSrcIdxTo(0);
    else if (currSrcIdx >= 0 && currSrcIdx < srcImgs.size() - 1)
        changeCurrSrcIdxTo(currSrcIdx + 1);
}

void InteractiveDigitalMontage::appendSourceImages()
{
    QString msg;
    switch (state)
    {
    case MainState::Labeling:
    case MainState::GradientFusing:
        msg = tr("Please Wait for Processing");
        lineEditSetText(ui.lineEditSrcImgs, msg, Qt::GlobalColor::red);
        return;
    default:
        break;
    }

    QStringList appendFileNames =
        QFileDialog::getOpenFileNames(
            this,
            tr("Append Source Images"),
            QCoreApplication::applicationDirPath(),
            "Images (*.png *.jpg)"
        );
    if (appendFileNames.size() == 0)
        return;
    appendFileNames.sort();

    for (auto fn : appendFileNames)
    {
        QImage img = QImage(fn);
        if (img.isNull())
        {
            clearSourceImages();
            QString msg = tr("Invalid Image File: ");
            msg += fn;
            lineEditSetText(ui.lineEditSrcImgs, msg, Qt::GlobalColor::red);
            return;
        }

        if (srcImgs.size() == 0)
        {
            imgW = img.width();
            imgH = img.height();
        }
        if (img.width() != imgW
            || img.height() != imgH)
        {
            clearSourceImages();
            QString msg = tr("Inconsistent Image Size, Caused by: ");
            msg += fn;
            lineEditSetText(ui.lineEditSrcImgs, msg, Qt::GlobalColor::red);
            return;
        }

        srcImgs.push_back(img);
        srcImgNames.push_back(fn);
        srcImgLblCols.push_back(
            QColor(rand() % 255, rand() % 255, rand() % 255)
        );
        designatedLbls.push_back(QImage()); // dummy op, aviod mem-outrange
    }

    changeCurrSrcIdxTo(0);
    state = MainState::SourceImageLoaded;
}

void InteractiveDigitalMontage::clearSourceImages()
{
    QString msg;
    switch (state)
    {
    case MainState::Labeling:
    case MainState::GradientFusing:
        msg = tr("Please Wait for Processing");
        lineEditSetText(ui.lineEditSrcImgs, msg, Qt::GlobalColor::red);
        return;
    default:
        break;
    }

    srcImgs.clear();
    srcImgNames.clear();
    srcImgLblCols.clear();
    designatedLbls.clear();
    changeCurrSrcIdxTo(-1);
    state = MainState::Initialized;
}

void InteractiveDigitalMontage::appendLoadedLabels()
{
    QString msg;
    switch (state)
    {
    case MainState::Initialized:
        msg = tr("Load Source Images First");
        lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
        return;
    case MainState::Labeling:
    case MainState::GradientFusing:
        msg = tr("Please Wait for Processing");
        lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
        return;
    default:
        break;
    }

    QStringList appendFileNames =
        QFileDialog::getOpenFileNames(
            this,
            tr("Append Loaded Labels"),
            QCoreApplication::applicationDirPath(),
            tr("Images (*.bmp)")
        );
    if (appendFileNames.size() == 0)
        return;

    QVector<QString> onlyNames;
    onlyNames.reserve(srcImgs.size());
    for (auto fn : srcImgNames)
    {
        int left = fn.lastIndexOf(QDir::separator()) + 1;
        int right = fn.lastIndexOf(QChar('.'));
        // no need to sort onlyNames here
        // since srcImgNames has been sorted
        onlyNames.push_back(fn.mid(left, right - left));
    }

    for (auto fn : appendFileNames)
    {
        int left = fn.lastIndexOf(QDir::separator()) + 1;
        int right = fn.lastIndexOf(QChar('.'));

        int srcImgIdx = onlyNames.indexOf(fn.mid(left, right - left));
        if (srcImgIdx == -1)
        {
            QString msg = tr("Inconsistent Label Name, Caused by: ");
            msg += fn;
            lineEditSetText(ui.lineEditIntrctLbls , msg, Qt::GlobalColor::red);
            return;
        }

        QImage img = QImage(fn);
        img.convertTo(QImage::Format::Format_RGB888); // necessary for further painting
        if (img.isNull())
        {
            QString msg = tr("Invalid Label File: ");
            msg += fn;
            lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
            return;
        }
        if (img.width() != imgW
            || img.height() != imgH)
        {
            QString msg = tr("Inconsistent Label Size, Caused by: ");
            msg += fn;
            lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
            return;
        }

        if (srcImgIdx >= designatedLbls.size())
            designatedLbls.resize(srcImgIdx + 1);
        designatedLbls[srcImgIdx] = img;
    }

    changeCurrSrcIdxTo(0);
    state = MainState::SourceImageLoaded;
}

void InteractiveDigitalMontage::appendInteractiveLabel()
{
    QString msg;
    switch (state)
    {
    case MainState::Initialized:
        msg = tr("Load Source Images First");
        lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
        return;
    case MainState::Labeling:
    case MainState::GradientFusing:
        msg = tr("Please Wait for Processing");
        lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
        return;
    default:
        break;
    }

    QPainterPath intrctPath = ui.graphicsViewIntrctLbls->getIntrctPath();
    
    if (designatedLbls[currSrcIdx].isNull())
    {
        // create a label image with all black pixels
        designatedLbls[currSrcIdx] = QImage(imgW, imgH, QImage::Format::Format_ARGB32);
        designatedLbls[currSrcIdx].fill(Qt::GlobalColor::black);
    }

    // paint interactive labels on designatedLbls[currSrcIdx]
    QPainter painter;
    QPen pen;
    pen.setColor(Qt::GlobalColor::white); // label is white in designatedLbls
    pen.setWidth(ui.verticalSliderStrokeWidth->value());
    painter.begin(&designatedLbls[currSrcIdx]);
    painter.setPen(pen);
    painter.setCompositionMode(QPainter::CompositionMode::CompositionMode_SourceOver);
    painter.drawPath(intrctPath);
    painter.end();

    // clear displayed interactive label
    ui.graphicsViewIntrctLbls->clearIntrctPath();

    // redisplay
    changeCurrSrcIdxTo(currSrcIdx);

    state = MainState::SourceImageLoaded;
}

void InteractiveDigitalMontage::eraseInteractiveLabel()
{
    QString msg;
    switch (state)
    {
    case MainState::Initialized:
        msg = tr("Load Source Images First");
        lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
        return;
    case MainState::Labeling:
    case MainState::GradientFusing:
        msg = tr("Please Wait for Processing");
        lineEditSetText(ui.lineEditIntrctLbls, msg, Qt::GlobalColor::red);
        return;
    default:
        break;
    }

    QPainterPath intrctPath = ui.graphicsViewIntrctLbls->getIntrctPath();

    if (designatedLbls[currSrcIdx].isNull())
        return; // no need to erase

    // erase interactive labels from designatedLbls[currSrcIdx]
    QPainter painter;
    QPen pen;
    pen.setColor(Qt::GlobalColor::black); // label is drawn black to be erased
    pen.setWidth(ui.verticalSliderStrokeWidth->value());
    painter.begin(&designatedLbls[currSrcIdx]);
    painter.setPen(pen);
    painter.setCompositionMode(QPainter::CompositionMode::CompositionMode_SourceOver);
    painter.drawPath(intrctPath);
    painter.end();

    // clear displayed interactive label
    ui.graphicsViewIntrctLbls->clearIntrctPath();

    // redisplay
    changeCurrSrcIdxTo(currSrcIdx);

    state = MainState::SourceImageLoaded;
}

void InteractiveDigitalMontage::clearDesignatedLabels()
{
    designatedLbls.clear();
    designatedLbls.resize(srcImgs.size());
    ui.graphicsViewIntrctLbls->clearForegroundImage();
}

void InteractiveDigitalMontage::clearCurrentLabel()
{
    designatedLbls[currSrcIdx] = QImage(); // set NULL
    ui.graphicsViewIntrctLbls->clearForegroundImage();
    ui.graphicsViewIntrctLbls->clearIntrctPath();
}

void InteractiveDigitalMontage::updateStrokeWidth()
{
    ui.graphicsViewIntrctLbls->configIntrctPath(
        true,
        ui.verticalSliderStrokeWidth->value(),
        srcImgLblCols[currSrcIdx]
    );
}

void InteractiveDigitalMontage::runLabelMatching()
{
    switch (state)
    {
    case MainState::Initialized:
        textEditSetText(
            ui.textEditLblMatchRslts, tr("Load Source Image First"),
            Qt::GlobalColor::red, false
        );
        return;
    case MainState::Labeling:
    case MainState::GradientFusing:
        textEditSetText(
            ui.textEditLblMatchRslts, tr("A Thread is Running Now, Please Wait"),
            Qt::GlobalColor::red, false
        );
        return;
    default:
        // enetr Labeling state
        // to avoid duplicate calls of this func
        textEditSetText(
            ui.textEditLblMatchRslts, tr("Start Label Matching"),
            Qt::GlobalColor::green, false
        );
        this->state = MainState::Labeling;
        break;
    }

    MontageLabelMatchWorker* worker =
        new MontageLabelMatchWorker(
            srcImgs, designatedLbls, srcImgLblCols,
            ui.doubleSpinBoxDatTermLrgPnlty->value(),
            ui.doubleSpinBoxDatTermAlpha->value(),
            ui.comboBoxSmoothTermType->currentIndex()
        );

    // run label match in another thread
    connect(worker, &MontageLabelMatchWorker::resultReady,
        this, &InteractiveDigitalMontage::handleLblMatchRslt);
    connect(worker, &MontageLabelMatchWorker::finished,
        worker, &QObject::deleteLater);
    worker->start(QThread::TimeCriticalPriority);
}

void InteractiveDigitalMontage::handleLblMatchRslt(const MontageLabelMatchResult& result)
{
    textEditSetText(
        ui.textEditLblMatchRslts, result.msg,
        Qt::GlobalColor::black, false
    );
    LMRslts[0] = result.expndLbl;
    LMRslts[1] = result.img;
    
    LMRslts[2] = result.expndLbl;

    if (!LMRslts[2].isNull())
    {
        QPainter painter;
        painter.begin(&LMRslts[2]);
        // set alpha of expanded colored label
        painter.setCompositionMode(QPainter::CompositionMode::CompositionMode_DestinationIn);
        painter.fillRect(LMRslts[2].rect(), QColor(0, 0, 0, 150));
        // draw image on label
        painter.setCompositionMode(QPainter::CompositionMode::CompositionMode_Overlay);
        painter.drawImage(0, 0, result.img);
        painter.end();

        LMRslts[3] = LMRslts[2];
        painter.begin(&LMRslts[3]);
        // draw colored label on previously producted image
        painter.setCompositionMode(QPainter::CompositionMode::CompositionMode_SourceOver);
        painter.drawImage(0, 0, result.colLbl);
        painter.end();
    }

    currLMRsltIdx = 0;
    ui.graphicsViewLblMatchRslts->loadBackgroudImage(LMRslts[currLMRsltIdx]);
    this->state = MainState::Labeled;
}

void InteractiveDigitalMontage::switchLblMatchRslts()
{
    currLMRsltIdx++;
    if (currLMRsltIdx == LMRsltNum)
        currLMRsltIdx = 0;
    ui.graphicsViewLblMatchRslts->loadBackgroudImage(LMRslts[currLMRsltIdx]);
}

void InteractiveDigitalMontage::exportLblMatchRslt()
{
    switch (state)
    {
    case MainState::Labeled:
    case MainState::GradientFusing:
    case MainState::GradientFused:
        break;
    default:
        textEditSetText(
            ui.textEditLblMatchRslts, tr("Export after 1 Label Match Result has been Generated"),
            Qt::GlobalColor::red, false
        );
        return;
    }

    if (LMRslts[currLMRsltIdx].isNull())
    {
        textEditSetText(
            ui.textEditLblMatchRslts, tr("NULL Result Generated during this Label Match Turn"),
            Qt::GlobalColor::red, false
        );
        return;
    }

    QString exportFileName =
        QFileDialog::getSaveFileName(
            this,
            tr("Save Label Match Result"),
            QCoreApplication::applicationDirPath(),
            tr("Images (*.png)")
        );
    if (exportFileName.isEmpty())return;

    if (!LMRslts[currLMRsltIdx].save(exportFileName))
    {
        textEditSetText(
            ui.textEditLblMatchRslts, tr("Save Image Failed"),
            Qt::GlobalColor::red, false
        );
    }
    else
        textEditSetText(
            ui.textEditLblMatchRslts, tr("Save Image Success"),
            Qt::GlobalColor::green, false
        );
}

void InteractiveDigitalMontage::runGradientFuse()
{
    switch (state)
    {
    case MainState::Initialized:
        textEditSetText(
            ui.textEditGradFuseRslts, tr("Load Source Image First"),
            Qt::GlobalColor::red, false
        );
        return;
    case MainState::SourceImageLoaded:
        textEditSetText(
            ui.textEditGradFuseRslts, tr("Run Label Match First"),
            Qt::GlobalColor::red, false
        );
        return;
    case MainState::Labeling:
    case MainState::GradientFusing:
        textEditSetText(
            ui.textEditGradFuseRslts, tr("A Thread is Running Now, Please Wait"),
            Qt::GlobalColor::red, false
        );
        return;
    default:
        // enetr Labeling state
        // to avoid duplicate calls of this func
        textEditSetText(
            ui.textEditGradFuseRslts, tr("Start Gradient Fusion"),
            Qt::GlobalColor::green, false
        );
        this->state = MainState::Labeling;
        break;
    }

    MontageGradientFusionWorker* worker =
        new MontageGradientFusionWorker(ui.comboBoxGradFuseSolver->currentIndex());

    // run gradient fusion in another thread
    connect(worker, &MontageGradientFusionWorker::resultReady,
        this, &InteractiveDigitalMontage::handleGradFuseRslt);
    connect(worker, &MontageGradientFusionWorker::finished,
        worker, &QObject::deleteLater);
    worker->start(QThread::TimeCriticalPriority);
}

void InteractiveDigitalMontage::handleGradFuseRslt(const MontageGradientFusionResult& result)
{
    textEditSetText(
        ui.textEditGradFuseRslts, result.msg,
        Qt::GlobalColor::black, false
    );
    GFRslt = result.img;
    ui.graphicsViewGradFuseRslts->loadBackgroudImage(GFRslt);
    this->state = MainState::GradientFused;
}

void InteractiveDigitalMontage::exportGradFuseRslt()
{
    switch (state)
    {
    case MainState::Labeled:
    case MainState::GradientFusing:
    case MainState::GradientFused:
        break;
    default:
        textEditSetText(
            ui.textEditGradFuseRslts, tr("Export after 1 Gradient Fusion Result has been Generated"),
            Qt::GlobalColor::red, false
        );
        return;
    }

    if (GFRslt.isNull())
    {
        textEditSetText(
            ui.textEditGradFuseRslts, tr("NULL Result Generated during this Gradient Fusion Turn"),
            Qt::GlobalColor::red, false
        );
        return;
    }

    QString exportFileName =
        QFileDialog::getSaveFileName(
            this,
            tr("Save Gradient Fusion Result"),
            QCoreApplication::applicationDirPath(),
            tr("Images (*.png)")
        );
    if (exportFileName.isEmpty())return;

    if (!GFRslt.save(exportFileName))
    {
        textEditSetText(
            ui.textEditGradFuseRslts, tr("Save Image Failed"),
            Qt::GlobalColor::red, false
        );
    }
    else
        textEditSetText(
            ui.textEditGradFuseRslts, tr("Save Image Success"),
            Qt::GlobalColor::green, false
        );
}

void InteractiveDigitalMontage::adjustSpinStepOnSmoothTypeChanged()
{
    switch (ui.comboBoxSmoothTermType->currentIndex())
    {
    case 0:
        ui.doubleSpinBoxDatTermLrgPnlty->setValue(1e8);
        ui.doubleSpinBoxDatTermAlpha->setValue(1.0);
        break;
    case 1:
        ui.doubleSpinBoxDatTermLrgPnlty->setValue(1000.0);
        ui.doubleSpinBoxDatTermAlpha->setValue(100.0);
        break;
    default:
        break;
    }
}

InteractiveDigitalMontage::InteractiveDigitalMontage(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    connect(ui.toolButtonAppendSrcImgs, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::appendSourceImages);
    connect(ui.toolButtonClearSrcImgs, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::clearSourceImages);
    connect(ui.toolButtonPreImg1, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::goToPreviousImage);
    connect(ui.toolButtonNextImg1, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::goToNextImage);

    connect(ui.toolButtonAppendLoadedLbls, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::appendLoadedLabels);
    connect(ui.toolButtonClearLbls, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::clearDesignatedLabels);
    connect(ui.toolButtonClearCurrLbl, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::clearCurrentLabel);
    connect(ui.toolButtonPreImg2, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::goToPreviousImage);
    connect(ui.toolButtonNextImg2, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::goToNextImage);
    connect(ui.toolButtonAppendIntrctLabl, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::appendInteractiveLabel);
    connect(ui.toolButtonEraseIntrctLabl, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::eraseInteractiveLabel);
    connect(ui.toolButtonUndo, &QToolButton::clicked,
        ui.graphicsViewIntrctLbls, &MontageGraphicsView::clearIntrctPath);
    connect(ui.verticalSliderStrokeWidth, &QSlider::valueChanged,
        this, &InteractiveDigitalMontage::updateStrokeWidth);

    connect(ui.toolButtonRunLblMatch, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::runLabelMatching);
    connect(ui.toolButtonSwitchLblMatchRslts, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::switchLblMatchRslts);
    connect(ui.toolButtonClearTextEditLblMatchRslts, &QToolButton::clicked,
        ui.textEditLblMatchRslts, &QTextEdit::clear);
    connect(ui.toolButtonExportLblMatchRslt, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::exportLblMatchRslt);

    connect(ui.toolButtonRunLblGradFuse, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::runGradientFuse);
    connect(ui.toolButtonClearTextEditGradFuseRslts, &QToolButton::clicked,
        ui.textEditGradFuseRslts, &QTextEdit::clear);
    connect(ui.toolButtonExportGradFuseRslt, &QToolButton::clicked,
        this, &InteractiveDigitalMontage::exportGradFuseRslt);

    connect(ui.comboBoxSmoothTermType, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &InteractiveDigitalMontage::adjustSpinStepOnSmoothTypeChanged);

    // maximize window
    setWindowState(Qt::WindowState::WindowMaximized);
    
    qRegisterMetaType<MontageLabelMatchResult>("MontageLabelMatchResult");
    qRegisterMetaType<MontageGradientFusionResult>("MontageGradientFusionResult");
}
