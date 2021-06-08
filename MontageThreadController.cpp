#include "MontageThreadController.h"

#include <QDebug>

#include "MontageCore.h"

// Modiified from:
//   https://blog.csdn.net/liyuanbhu/article/details/46662115
static cv::Mat qImage2CvMat(const QImage& image)
{
	cv::Mat mat;
	qDebug() << image.format();
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

void MontageLabelMatchWorker::run()
{
	using namespace std;
	using namespace cv;

	string stdMsg;
	Mat rsltLbl, rsltImg;
	MontageCore mc;
	mc.BindResult(&stdMsg, &rsltLbl, &rsltImg);
	mc.Run(images, label);

	Result rslt = {
		QString::fromStdString(stdMsg),

	};
	emit resultReady(rslt);
}

MontageLabelMatchWorker::MontageLabelMatchWorker(
	const QVector<QImage>& images,
	const QVector<QImage>& labels)
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
						this->label.at<uchar>(y, x) = srcImgIdx;

			break;
		}
		srcImgIdx++;
	}
}