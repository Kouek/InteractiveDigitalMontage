#include "MontageCore.h"
#include "GCoptimization.h"
#include <Eigen/Core>
#include <Eigen/Sparse>

#include <sstream>

#include <QDebug>

using namespace cv;


const double large_penalty = 1000.0;
Mat _data;

struct __ExtraData
{
	std::vector<cv::Mat> Images;
	std::vector<cv::Mat> GrayImages;
	cv::Mat Label;
	cv::flann::Index* kdtree;
};

GCoptimization::EnergyType dataFn(int p, int l, void* data)
{
	__ExtraData* ptr_extra_data = (__ExtraData*)data;
	cv::Mat& Label = ptr_extra_data->Label;
	//cv::flann::Index * ptr_kdtree = ptr_extra_data->kdtree;

	int width = Label.cols;
	int height = Label.rows;

	int y = p / width;
	int x = p % width;

	assert(l >= 0);
	if (Label.at<char>(y, x) != MontageCore::undefined)	// user specified
	{
		if (Label.at<char>(y, x) == l)
		{
			return 0.0;
		}
		else
		{
			return large_penalty;
		}
	}
	else
	{
		return large_penalty;
	}
}

GCoptimization::EnergyType euc_dist(const Vec3b& a, const Vec3b& b)
{
	Vec3d double_diff = a - b;
	return sqrt(double_diff[0] * double_diff[0] + double_diff[1] * double_diff[1] + double_diff[2] * double_diff[2]);
}

cv::Vec2d sobel_gradient(const cv::Mat& grayImg, int x, int y)
{
	double ex = 0, ey = 0;
	if (x != 0 && x != grayImg.cols - 1
		&& y != 0 && y != grayImg.rows - 1)
	{
		// y direction
		ey -= grayImg.at<uchar>(y - 1, x - 1) +
			2.0 * grayImg.at<uchar>(y - 1, x) +
			grayImg.at<uchar>(y - 1, x + 1);
		ey += grayImg.at<uchar>(y + 1, x - 1) +
			2.0 * grayImg.at<uchar>(y + 1, x) +
			grayImg.at<uchar>(y + 1, x + 1);
		// x direction
		ex -= grayImg.at<uchar>(y - 1, x - 1) +
			2.0 * grayImg.at<uchar>(y, x - 1) +
			grayImg.at<uchar>(y + 1, x - 1);
		ex += grayImg.at<uchar>(y - 1, x + 1) +
			2.0 * grayImg.at<uchar>(y, x + 1) +
			grayImg.at<uchar>(y + 1, x + 1);
	}
	return cv::Vec2d(ex, ey);
}

double edge_potential(const cv::Mat& grayImg, int xp, int yp, int xq, int yq)
{
	Vec2d e0 = sobel_gradient(grayImg, xp, yp);
	Vec2d e1 = sobel_gradient(grayImg, xq, yq);
	e1 = e1 - e0;
	return sqrt(e1[0] * e1[0] + e1[1] * e1[1]);
}

double smoothFn(int p, int q, int lp, int lq, void* data)
{
	if (lp == lq)
	{
		return 0.0;
	}

	assert(lp != lq);
	__ExtraData* ptr_extra_data = (__ExtraData*)data;
	cv::Mat& Label = ptr_extra_data->Label;
	std::vector<cv::Mat>& Images = ptr_extra_data->Images;
	std::vector<cv::Mat>& GrayImages = ptr_extra_data->GrayImages;
	int n_label = Images.size();
	assert(lp < n_label&& lq < n_label);

	int width = Label.cols;
	int height = Label.rows;

	int yp = p / width;
	int xp = p % width;
	int yq = q / width;
	int xq = q % width;

	double X_term1 = euc_dist(Images[lp].at<Vec3b>(yp, xp), Images[lq].at<Vec3b>(yp, xp));
	double X_term2 = euc_dist(Images[lp].at<Vec3b>(yq, xq), Images[lq].at<Vec3b>(yq, xq));
	//double X_term1 = euc_dist(Images[lp].at<Vec3b>(yp, xp), Images[lq].at<Vec3b>(yq, xq));
	//double X_term2 = euc_dist(Images[lp].at<Vec3b>(yq, xq), Images[lq].at<Vec3b>(yp, xp));
	assert(X_term1 + X_term1 >= 0.0);

	return (X_term1 + X_term2) * 100;

	double Z_term1 = edge_potential(GrayImages[lp], xp, yp, xq, yq);
	double Z_term2 = edge_potential(GrayImages[lq], xp, yp, xq, yq);

	double ret = (X_term1 + X_term2) / 255.0 / (Z_term1 + Z_term2);
	if (ret > GCO_MAX_ENERGYTERM)
		ret = GCO_MAX_ENERGYTERM;
	return ret;
}


void MontageCore::TryAppendLMResultMsg(const std::string& str)
{
	if (LMResultMsg == nullptr)return;
	LMResultMsg->append(str + "\n");
}

static void TrySetResultMat(cv::Mat* mat, const cv::Mat& right)
{
	if (mat == nullptr)return;
	(*mat) = right;
}

void MontageCore::Run(const std::vector<cv::Mat>& Images, const cv::Mat& Label)
{
	assert(Images[0].rows == Label.rows);
	assert(Images[0].cols == Label.cols);

	BuildSolveMRF(Images, Label);
}

void MontageCore::BindResult(std::string* LMResultMsg, cv::Mat* LMResultLabel, cv::Mat* LMResultImage)
{
	this->LMResultMsg = LMResultMsg;
	if (this->LMResultMsg != nullptr)
		this->LMResultMsg->clear();
	this->LMResultLabel = LMResultLabel;
	this->LMResultImage = LMResultImage;
}

void MontageCore::BindImageColors(const std::vector<Vec3b>* ImageColors)
{
	this->ImageColors = ImageColors;
}

void MontageCore::BuildSolveMRF(const std::vector<cv::Mat>& Images, const cv::Mat& Label)
{
	const int n_imgs = Images.size();
	__ExtraData extra_data;
	extra_data.Images.resize(n_imgs);
	extra_data.GrayImages.resize(n_imgs);
	for (int i = 0; i < n_imgs; i++)
	{
		extra_data.Images[i] = Images[i];
		cvtColor(Images[i], extra_data.GrayImages[i], CV_RGB2GRAY);
	}
	extra_data.Label = Label;
	//extra_data.kdtree = AddInertiaConstraint( Label );
	int width = Label.cols;
	int height = Label.rows;
	int n_label = n_imgs;

	GCoptimizationGridGraph* gc = new GCoptimizationGridGraph(width, height, n_imgs);
	try
	{
		// set up the needed data to pass to function for the data costs
		gc->setDataCost(&dataFn, &extra_data);

		// smoothness comes from function pointer
		gc->setSmoothCost(&smoothFn, &extra_data);

		std::string prnt = "Before optimization energy is ";
		TryAppendLMResultMsg(
			prnt + std::to_string(gc->compute_energy())
		);

		printf("\nBefore optimization energy is %f", gc->compute_energy());
		//gc->swap(n_label * 2);// run expansion for 2 iterations. For swap use gc->swap(num_iterations);
		gc->expansion(2);
		printf("\nAfter optimization energy is %f", gc->compute_energy());

		prnt = "After optimization energy is ";
		TryAppendLMResultMsg(
			prnt + std::to_string(gc->compute_energy())
		);

		Mat result_label(height, width, CV_8UC1);

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				int idx = y * width + x;

				result_label.at<uchar>(y, x) = gc->whatLabel(idx);
			}
		}
		delete gc;

		VisResultLabelMap(result_label, n_label);
		VisCompositeImage(result_label, Images);

		//BuildSolveGradientFusion(Images, result_label);;
	}
	catch (GCException e)
	{
		e.Report();
		TryAppendLMResultMsg(e.message);
		delete gc;
	}
}

void MontageCore::GradientAt(const cv::Mat& Image, int x, int y, cv::Vec3f& grad_x, cv::Vec3f& grad_y)
{

	Vec3i color1 = Image.at<Vec3b>(y, x);
	Vec3i color2 = Image.at<Vec3b>(y, x + 1);
	Vec3i color3 = Image.at<Vec3b>(y + 1, x);
	grad_x = color2 - color1;
	grad_y = color3 - color1;

}

void MontageCore::BuildSolveGradientFusion(const std::vector<cv::Mat>& Images, const cv::Mat& ResultLabel)
{

	int width = ResultLabel.cols;
	int height = ResultLabel.rows;
	Mat color_result(height, width, CV_8UC3);
	Mat color_gradient_x(height, width, CV_32FC3);
	Mat color_gradient_y(height, width, CV_32FC3);

	for (int y = 0; y < height - 1; y++)
	{
		for (int x = 0; x < width - 1; x++)
		{
			GradientAt(Images[ResultLabel.at<uchar>(y, x)], x, y, color_gradient_x.at<Vec3f>(y, x), color_gradient_y.at<Vec3f>(y, x));
		}
	}


	Vec3b color0 = Images[0].at<Vec3b>(0, 0);
	SolveChannel(0, color0[0], color_gradient_x, color_gradient_y, color_result);
	SolveChannel(1, color0[1], color_gradient_x, color_gradient_y, color_result);
	SolveChannel(2, color0[2], color_gradient_x, color_gradient_y, color_result);


	/*imshow("color result", color_result);
	waitKey(0);*/

}

void MontageCore::VisResultLabelMap(const cv::Mat& ResultLabel, int n_label)
{
	int width = ResultLabel.cols;
	int height = ResultLabel.rows;
	Mat color_result_map(height, width, CV_8UC3);
	std::vector<Vec3b> label_colors;
	if (ImageColors == nullptr)
		for (int i = 0; i < n_label; i++)
		{
			label_colors.push_back(Vec3b(rand() % 256, rand() % 256, rand() % 256));
		}
	else
		label_colors = *ImageColors;
	//label_colors.push_back(Vec3b(255,0,0));
	//label_colors.push_back(Vec3b(0,255,0));
	//label_colors.push_back(Vec3b(0,0,255));
	//label_colors.push_back(Vec3b(255,255,0));
	//label_colors.push_back(Vec3b(0,255,255));


	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			color_result_map.at<Vec3b>(y, x) = label_colors[ResultLabel.at<uchar>(y, x)];
		}
	}

	TrySetResultMat(LMResultLabel, color_result_map);
	/*imshow("result labels", color_result_map);
	waitKey(27);*/
}

void MontageCore::VisCompositeImage(const cv::Mat& ResultLabel, const std::vector<cv::Mat>& Images)
{
	int width = ResultLabel.cols;
	int height = ResultLabel.rows;
	Mat composite_image(height, width, CV_8UC3);

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			composite_image.at<Vec3b>(y, x) = Images[ResultLabel.at<uchar>(y, x)].at<Vec3b>(y, x);
		}
	}

	TrySetResultMat(LMResultImage, composite_image);
	/*imshow("composite image", composite_image);
	waitKey(27);*/
}

cv::flann::Index* MontageCore::AddInertiaConstraint(const cv::Mat& Label)
{
	int height = Label.rows;
	int width = Label.cols;
	std::vector<Point2f> _data_vec;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			if (Label.at<char>(y, x) > 0)
			{
				_data_vec.push_back(Point2f(x, y));
			}
		}
	}

	_data.create(_data_vec.size(), 2, CV_32FC1);
	for (int i = 0; i < _data_vec.size(); i++)
	{
		_data.at<float>(i, 0) = _data_vec[i].x;
		_data.at<float>(i, 1) = _data_vec[i].y;
	}
	cv::flann::KDTreeIndexParams indexParams;
	return new cv::flann::Index(_data, indexParams);

	//std::vector<int> indices(1); 
	//std::vector<float> dists(1); 
	//Mat query(1,2,CV_32FC1);
	//query.at<float>(0,0) = 522;
	//query.at<float>(0,1) = 57;
	//kdtree->knnSearch(query, indices, dists, 1,cv::flann::SearchParams(64)); 
}

void MontageCore::SolveChannel(int channel_idx, int constraint, const cv::Mat& color_gradient_x, const cv::Mat& color_gradient_y, cv::Mat& output)
{
#if 1
	int width = color_gradient_x.cols;
	int height = color_gradient_x.rows;


	int NumOfUnknownTerm = 2 * width * height + 1;
	std::vector<Eigen::Triplet<double>> NonZeroTerms;
	Eigen::VectorXd b(NumOfUnknownTerm);
	for (int y = 0; y < height - 1; y++)
	{
		for (int x = 0; x < width - 1; x++)
		{
			int unknown_idx = y * width + x;

			/// gradient x
			int eq_idx1 = 2 * unknown_idx;
			NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx1, unknown_idx, -1));
			int other_idx1 = y * width + (x + 1);
			NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx1, other_idx1, 1));
			Vec3f grads_x = color_gradient_x.at<Vec3f>(y, x);
			b(eq_idx1) = grads_x[channel_idx];

			/// gradient y
			int eq_idx2 = 2 * unknown_idx + 1;
			NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx2, unknown_idx, -1));
			int other_idx2 = (y + 1) * width + x;
			NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx2, other_idx2, 1));
			Vec3f grads_y = color_gradient_y.at<Vec3f>(y, x);
			b(eq_idx2) = grads_y[channel_idx];
		}
	}


	///constraint
	int eq_idx = width * height * 2;
	NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx, 0, 1));
	b(eq_idx) = constraint;



	Eigen::SparseMatrix<double> A(NumOfUnknownTerm, width * height);
	A.setFromTriplets(NonZeroTerms.begin(), NonZeroTerms.end());
	NonZeroTerms.erase(NonZeroTerms.begin(), NonZeroTerms.end());

	Eigen::SparseMatrix<double> ATA(width * height, width * height);
	ATA = A.transpose() * A;
	Eigen::VectorXd ATb = A.transpose() * b;

	printf("\nSolving...\n");
	Eigen::ConjugateGradient<Eigen::SparseMatrix<double>> CGSolver(ATA);
	Eigen::VectorXd solution = CGSolver.solve(ATb);
	printf("Solved!\n");

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			Vec3b& temp = output.at<Vec3b>(y, x);
			temp[channel_idx] = uchar(std::max(std::min(solution(y * width + x), 255.0), 0.0));
			//printf("%d,%d,  %f, %f\n",y,x, solution(y * width + x), ATb(y*width + x));
			//system("pause");
		}
	}

	//imshow("output",output);
	//waitKey(0);
#endif

	///请同学们填写这里的代码，这里就是实验中所说的单颜色通道的Gradient Fusion
}
