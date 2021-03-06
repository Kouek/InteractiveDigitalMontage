#include "MontageThreadController.h"

#include <QDebug>

// Modiified from:
//   https://blog.csdn.net/liyuanbhu/article/details/46662115
static cv::Mat qImage2CvMat(const QImage& image)
{
	cv::Mat mat;
	switch (image.format())
	{
	case QImage::Format_ARGB32:
	case QImage::Format_RGB32:
	case QImage::Format_ARGB32_Premultiplied:
		mat = cv::Mat(image.height(), image.width(), CV_8UC4,
			(void*)image.constBits(), image.bytesPerLine());
		cv::cvtColor(mat, mat, CV_RGBA2RGB); // discard alpha
		break;
	case QImage::Format_RGB888:
		mat = cv::Mat(image.height(), image.width(), CV_8UC3,
			(void*)image.constBits(), image.bytesPerLine());
		cv::cvtColor(mat, mat, CV_BGR2RGB);
		break;
	case QImage::Format_Indexed8:
		mat = cv::Mat(image.height(), image.width(), CV_8UC1,
			(void*)image.constBits(), image.bytesPerLine());
		break;
	}
	return mat;
}

// Modified from:
//   https://blog.csdn.net/liyuanbhu/article/details/46662115
static QImage cvMat2QImage(const cv::Mat& mat)
{
	using namespace cv;
	// 8-bits unsigned, NO. OF CHANNELS = 1
	if (mat.type() == CV_8UC1)
	{
		QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
		// Set the color table (used to translate colour indexes to qRgb values)
		image.setColorCount(256);
		for (int i = 0; i < 256; i++)
		{
			image.setColor(i, qRgb(i, i, i));
		}
		// Copy input Mat
		uchar* pSrc = mat.data;
		for (int row = 0; row < mat.rows; row++)
		{
			uchar* pDest = image.scanLine(row);
			memcpy(pDest, pSrc, mat.cols);
			pSrc += mat.step;
		}
		return image;
	}
	// 8-bits unsigned, NO. OF CHANNELS = 3
	else if (mat.type() == CV_8UC3)
	{
		// Copy input Mat
		const uchar* pSrc = (const uchar*)mat.data;
		// Create QImage with same dimensions as input Mat
		QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
		return image.rgbSwapped();
	}
	else if (mat.type() == CV_8UC4)
	{
		qDebug() << "CV_8UC4";
		// Copy input Mat
		const uchar* pSrc = (const uchar*)mat.data;
		// Create QImage with same dimensions as input Mat
		QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
		return image.copy();
	}
	else
	{
		qDebug() << "ERROR: Mat could not be converted to QImage.";
		return QImage();
	}
}

void MontageLabelMatchWorker::run()
{
	using namespace std;
	using namespace cv;

	string stdMsg;
	Mat rsltLbl, rsltImg;
	MontageCore mc;
	mc.BindResult(&stdMsg, &rsltLbl, &rsltImg);
	mc.BindImageColors(&imageColors);
	mc.RunLabelMatch(images, label, largePenalty, smoothAlpha, smoothType);
	
	MontageLabelMatchResult rslt = {
		QString::fromStdString(stdMsg),
		cvMat2QImage(rsltLbl),
		cvMat2QImage(rsltImg),
		this->colLabel
	};
	emit resultReady(rslt);
}

MontageLabelMatchWorker::MontageLabelMatchWorker(
	const QVector<QImage>& images,
	const QVector<QImage>& labels,
	const QVector<QColor>& imageColors,
	double largePenalty, double smoothAlpha, int smoothType
	)
{
	using namespace std;
	using namespace cv;
	// init images
	for (auto img : images)
	{
		this->images.push_back(qImage2CvMat(img));
	}
	
	// init label
	int height, width;
	label.create(
		height = this->images[0].rows,
		width = this->images[0].cols,
		CV_8SC1
	);
	label.setTo(MontageCore::undefined);

	// init colLabel
	colLabel = QImage(width, height, QImage::Format::Format_ARGB32);
	colLabel.fill(QColor(0, 0, 0, 0));

	int srcImgIdx = 0;
	for (auto lbl : labels)
	{
		// use for(;;) to simplify coding
		for (;;)
		{
			if (lbl.isNull())break;
			
			for (int y = 0; y < height; y++)
				for (int x = 0; x < width; x++)
					if (lbl.pixelColor(x, y) != Qt::GlobalColor::black)
					{
						this->label.at<uchar>(y, x) = srcImgIdx;
						this->colLabel.setPixelColor(x, y, imageColors[srcImgIdx]);
					}

			break;
		}
		srcImgIdx++;
	}

	// init imageColors
	for (auto col : imageColors)
	{
		// here swap RGB 2 BGR
		this->imageColors.push_back(
			Vec3b(
				col.blue(),
				col.green(),
				col.red()
			)
		);
	}

	// init config
	this->largePenalty = largePenalty;
	this->smoothAlpha = smoothAlpha;
	switch (smoothType)
	{
	case 0:
		this->smoothType = MontageCore::SmoothTermType::X;
		break;
	case 1:
		this->smoothType = MontageCore::SmoothTermType::X_Plus_Y;
		break;
	case 2:
	default:
		this->smoothType = MontageCore::SmoothTermType::X_Divide_By_Z;
		break;
	}
}

void MontageGradientFusionWorker::run()
{
	using namespace std;
	using namespace cv;

	string stdMsg;
	Mat rsltImg;
	MontageCore mc;
	mc.BindResult(&stdMsg, nullptr, &rsltImg);
	mc.RunGradientFusion(solverType);
	
	MontageGradientFusionResult rslt =
	{
		QString::fromStdString(stdMsg),
		cvMat2QImage(rsltImg)
	};

	emit resultReady(rslt);
}

MontageGradientFusionWorker::MontageGradientFusionWorker(int solverType)
{
	switch (solverType)
	{
	case 0:
		this->solverType = MontageCore::GradientFusionSolverType::My_Solver;
		break;
	default:
		this->solverType = MontageCore::GradientFusionSolverType::Eigen_Solver;
		break;
	}
}
