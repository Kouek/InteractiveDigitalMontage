#include "MontageCore.h"
#include "GCoptimization.h"
#include <Eigen/Core>
#include <Eigen/Sparse>
#include "SparseMat.h"

#include <sstream>

using namespace cv;

static double large_penalty = 1e8; // can be modified by user
static double smooth_alpha = 100.0; // can be modified by user
// can be chosen by user
static MontageCore::SmoothTermType smooth_type = MontageCore::SmoothTermType::X;
// can be chosen by user
static MontageCore::GradientFusionSolverType solver_type = MontageCore::GradientFusionSolverType::Eigen_Solver;

// buffered images
// generated from last Label Match
// to be used in successive Gradient Fusion
static std::vector<cv::Mat> BufImages;
// buffered result label
static cv::Mat BufResultLabel;

Mat _data;

struct __ExtraData
{
	std::vector<cv::Mat> Images;
	std::vector<cv::Mat> XGrads[3];
	std::vector<cv::Mat> YGrads[3];
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
			return 0.0;
		else
			return large_penalty;
	}
	else
		return large_penalty;
}

static GCoptimization::EnergyType euc_dist(const Vec3b& a, const Vec3b& b)
{
	Vec3d double_diff = a - b;
	return sqrt(double_diff[0] * double_diff[0] + double_diff[1] * double_diff[1] + double_diff[2] * double_diff[2]);
}

static GCoptimization::EnergyType six_comp_grad(
	int xp, int yp, int xq, int yq, int lp, int lq,
	const std::vector<cv::Mat>* XGrads, std::vector<cv::Mat>* YGrads)
{
	GCoptimization::EnergyType double_diff;
	Vec3b vlp = Vec3b(
		YGrads[0][lp].at<uchar>(yp, xp),
		YGrads[1][lp].at<uchar>(yp, xp),
		YGrads[2][lp].at<uchar>(yp, xp)
	);
	Vec3b vlq = Vec3b(
		YGrads[0][lq].at<uchar>(yp, xp),
		YGrads[1][lq].at<uchar>(yp, xp),
		YGrads[2][lq].at<uchar>(yp, xp)
	);
	double_diff = euc_dist(vlp, vlq);

	vlp = Vec3b(
		YGrads[0][lp].at<uchar>(yq, xq),
		YGrads[1][lp].at<uchar>(yq, xq),
		YGrads[2][lp].at<uchar>(yq, xq)
	);
	vlq = Vec3b(
		YGrads[0][lq].at<uchar>(yq, xq),
		YGrads[1][lq].at<uchar>(yq, xq),
		YGrads[2][lq].at<uchar>(yq, xq)
	);
	double_diff += euc_dist(vlp, vlq);

	vlp = Vec3b(
		XGrads[0][lp].at<uchar>(yp, xp),
		XGrads[1][lp].at<uchar>(yp, xp),
		XGrads[2][lp].at<uchar>(yp, xp)
	);
	vlq = Vec3b(
		XGrads[0][lq].at<uchar>(yp, xp),
		XGrads[1][lq].at<uchar>(yp, xp),
		XGrads[2][lq].at<uchar>(yp, xp)
	);
	double_diff += euc_dist(vlp, vlq);

	vlp = Vec3b(
		XGrads[0][lp].at<uchar>(yq, xq),
		XGrads[1][lp].at<uchar>(yq, xq),
		XGrads[2][lp].at<uchar>(yq, xq)
	);
	vlq = Vec3b(
		XGrads[0][lq].at<uchar>(yq, xq),
		XGrads[1][lq].at<uchar>(yq, xq),
		XGrads[2][lq].at<uchar>(yq, xq)
	);
	double_diff += euc_dist(vlp, vlq);
	return double_diff;
}

static GCoptimization::EnergyType edge_potential(int x, int y,
	const cv::Mat& rGrad, const cv::Mat& gGrad, const cv::Mat& bGrad)
{
	GCoptimization::EnergyType r, g, b;
	r = rGrad.at<uchar>(y, x);
	g = gGrad.at<uchar>(y, x);
	b = bGrad.at<uchar>(y, x);
	return sqrt(r * r + g * g + b * b);
}

double smoothFn(int p, int q, int lp, int lq, void* data)
{
	if (lp == lq)
	{
		return 0.0;
	}

	__ExtraData* ptr_extra_data = (__ExtraData*)data;
	cv::Mat& Label = ptr_extra_data->Label;
	std::vector<cv::Mat>& Images = ptr_extra_data->Images;
	std::vector<cv::Mat>* XGrads = ptr_extra_data->XGrads;
	std::vector<cv::Mat>* YGrads = ptr_extra_data->YGrads;
	int n_label = Images.size();
	assert(lp < n_label&& lq < n_label);

	int width = Label.cols;
	int height = Label.rows;

	int yp = p / width;
	int xp = p % width;
	int yq = q / width;
	int xq = q % width;

	double X_term = euc_dist(Images[lp].at<Vec3b>(yp, xp), Images[lq].at<Vec3b>(yp, xp));
	X_term += euc_dist(Images[lp].at<Vec3b>(yq, xq), Images[lq].at<Vec3b>(yq, xq));
	X_term *= smooth_alpha;

	if (smooth_type == MontageCore::SmoothTermType::X)
		return X_term;

	if (smooth_type == MontageCore::SmoothTermType::X_Plus_Y)
	{
		double Y_term = six_comp_grad(xp, yp, xq, yq, lp, lq, XGrads, YGrads);
		Y_term = X_term + Y_term;
		return Y_term;
	}

	double Z_term = 0.0;
	if (xp != xq)
	{
		// pixel p and q are horizonal neighbors
		// find whether p-q is on an edge by calc vertical grad
		int minX = xp < xq ? xp : xq;
		Z_term = edge_potential(minX, yp, YGrads[0][lp], YGrads[1][lp], YGrads[2][lp]);
		Z_term += edge_potential(minX, yp, YGrads[0][lq], YGrads[1][lq], YGrads[2][lq]);
	}
	else
	{
		// pixel p and q are vertical neighbors
		// find whether p-q is on an edge by calc horizonal grad
		int minY = yp < yq ? yp : yq;
		Z_term = edge_potential(xp, minY, XGrads[0][lp], XGrads[1][lp], XGrads[2][lp]);
		Z_term += edge_potential(xp, minY, XGrads[0][lq], XGrads[1][lq], XGrads[2][lq]);
	}
	Z_term = X_term / Z_term;

	if (Z_term > large_penalty || Z_term != Z_term)
		return large_penalty;
	return Z_term;
}

// used to return message to GUI
void TryAppendResultMsg(std::string* msg, const std::string& str)
{
	if (msg == nullptr)return;
	msg->append(str + "\n");
}

// used to return image or label to GUI
static void TrySetResultMat(cv::Mat* mat, const cv::Mat& right)
{
	if (mat == nullptr)return;
	(*mat) = right;
}

void MontageCore::RunLabelMatch(const std::vector<cv::Mat>& Images, const cv::Mat& Label,
	double LargePenalty, double SmoothAlpha, SmoothTermType SmoothType)
{
	large_penalty = LargePenalty;
	smooth_alpha = SmoothAlpha;
	smooth_type = SmoothType;
	BuildSolveMRF(Images, Label);
}

void MontageCore::RunGradientFusion(GradientFusionSolverType SolverType)
{
	solver_type = SolverType;
	BuildSolveGradientFusion(BufImages, BufResultLabel);
}

void MontageCore::BindResult(std::string* ResultMsg, cv::Mat* ResultLabel, cv::Mat* ResultImage)
{
	this->ResultMsg = ResultMsg;
	if (this->ResultMsg != nullptr)
		this->ResultMsg->clear();
	this->ResultLabel = ResultLabel;
	this->ResultImage = ResultImage;
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
	for (int i = 0; i < 3; i++)
	{
		extra_data.XGrads[i].resize(n_imgs);
		extra_data.YGrads[i].resize(n_imgs);
	}
	std::vector<cv::Mat> rgbMats(3);
	for (int i = 0; i < n_imgs; i++)
	{
		extra_data.Images[i] = Images[i];
		
		cv::split(Images[i], rgbMats);

		assert(rgbMats.size() == 3);
		for (int c = 0; c < 3; c++)
		{
			cv::Sobel(rgbMats[c], extra_data.XGrads[c][i], -1, 1, 0);
			cv::Sobel(rgbMats[c], extra_data.YGrads[c][i], -1, 0, 1);
		}
	}
	extra_data.Label = Label;
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
		TryAppendResultMsg(
			ResultMsg,
			prnt + std::to_string(gc->compute_energy())
		);

		printf("\nBefore optimization energy is %f", gc->compute_energy());
		if (smooth_type == MontageCore::SmoothTermType::X_Divide_By_Z)
			gc->swap(n_label * 2);// run expansion for 2 iterations. For swap use gc->swap(num_iterations);
		else
			gc->expansion(2);
		printf("\nAfter optimization energy is %f", gc->compute_energy());

		prnt = "After optimization energy is ";
		TryAppendResultMsg(
			ResultMsg,
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

		// buffer
		BufImages = Images;
		BufResultLabel = result_label;

		VisResultLabelMap(result_label, n_label);
		VisCompositeImage(result_label, Images);
	}
	catch (GCException e)
	{
		e.Report();
		TryAppendResultMsg(ResultMsg, e.message);
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

// used in funcs avgConstraintVec3b, BuildSolveGradientFusion and SolveChannel
// state the position of constrant point
static int constraintX, constraintY;

static Vec3b avgConstraintVec3b(const std::vector<cv::Mat>& Images)
{
	Vec3b v3b;
	Vec3d v3db(0, 0, 0);
	double n_img = (double)Images.size();
	for (auto img : Images)
	{
		v3b = img.at<Vec3b>(constraintY, constraintX);
		v3db[0] += (double)v3b[0] / n_img;
		v3db[1] += (double)v3b[1] / n_img;
		v3db[2] += (double)v3b[2] / n_img;
	}
	v3b[0] = (uchar)std::max(0.0, std::min(255.0, v3db[0]));
	v3b[1] = (uchar)std::max(0.0, std::min(255.0, v3db[1]));
	v3b[2] = (uchar)std::max(0.0, std::min(255.0, v3db[2]));
	return v3b;
}

// used in func prepareAandATA and SolveChannel
// declare here to avoid duplicate cpmputation
Eigen::SparseMatrix<double> A;
Eigen::SparseMatrix<double> ATA;
Kouek::SparseMat<double> myA;
Kouek::SparseMat<double> myATA;

static void prepareAandATA(int height, int width)
{
	int NumOfUnknownTerm = 2 * width * height + 1;
	if (solver_type == MontageCore::GradientFusionSolverType::Eigen_Solver)
	{
		std::vector<Eigen::Triplet<double>> NonZeroTerms;
		for (int y = 0; y < height - 1; y++)
		{
			for (int x = 0; x < width - 1; x++)
			{
				int unknown_idx = y * width + x;

				// gradient x
				int eq_idx1 = 2 * unknown_idx;
				NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx1, unknown_idx, -1));
				int other_idx1 = y * width + (x + 1);
				NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx1, other_idx1, 1));

				// gradient y
				int eq_idx2 = 2 * unknown_idx + 1;
				NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx2, unknown_idx, -1));
				int other_idx2 = (y + 1) * width + x;
				NonZeroTerms.push_back(Eigen::Triplet<double>(eq_idx2, other_idx2, 1));
			}
		}

		///constraint
		int eq_idx = width * height * 2;
		NonZeroTerms.push_back(Eigen::Triplet<double>(
			eq_idx, constraintY * width + constraintX, 1)
		);

		A = Eigen::SparseMatrix<double>(NumOfUnknownTerm, width * height);
		A.setFromTriplets(NonZeroTerms.begin(), NonZeroTerms.end());
		NonZeroTerms.erase(NonZeroTerms.begin(), NonZeroTerms.end());

		ATA = Eigen::SparseMatrix<double>(width * height, width * height);
		ATA = A.transpose() * A;
	}
	else if (solver_type == MontageCore::GradientFusionSolverType::My_Solver)
	{
		using namespace std;

		int numOfUnknown = 2 * width * height + 1;
		vector<double> b(numOfUnknown);

		vector<int> nonZeroXs, nonZeroYs;
		vector<double> nonZeroTerms;
		for (int y = 0; y < height - 1; y++)
		{
			for (int x = 0; x < width - 1; x++)
			{
				int idx = y * width + x;
				// gradient X
				int eq_idx = 2 * idx;
				nonZeroYs.push_back(eq_idx);
				nonZeroXs.push_back(idx);
				nonZeroTerms.push_back(-1.0);
				int other_idx = y * width + (x + 1);
				nonZeroYs.push_back(eq_idx);
				nonZeroXs.push_back(other_idx);
				nonZeroTerms.push_back(1.0);

				// gradient Y
				eq_idx = 2 * idx + 1;
				nonZeroYs.push_back(eq_idx);
				nonZeroXs.push_back(idx);
				nonZeroTerms.push_back(-1.0);
				other_idx = (y + 1) * width + x;
				nonZeroYs.push_back(eq_idx);
				nonZeroXs.push_back(other_idx);
				nonZeroTerms.push_back(1.0);
			}
		}

		nonZeroYs.push_back(numOfUnknown - 1);
		nonZeroXs.push_back(0);
		nonZeroTerms.push_back(1.0);

		myA = Kouek::SparseMat<double>::initializeFromVector(
			nonZeroYs, nonZeroXs, nonZeroTerms, nonZeroTerms.size()
		); // A 2m*m
		myATA = myA.ATA(); // ATA m*m
	}
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

	constraintX = constraintY = 0;
	//Vec3b color0 = Images[0].at<Vec3b>(constraintY, constraintX);
	Vec3b color0 = avgConstraintVec3b(Images);
	prepareAandATA(color_gradient_x.rows, color_gradient_x.cols);
	SolveChannel(0, color0[0], color_gradient_x, color_gradient_y, color_result);
	SolveChannel(1, color0[1], color_gradient_x, color_gradient_y, color_result);
	SolveChannel(2, color0[2], color_gradient_x, color_gradient_y, color_result);

	TrySetResultMat(this->ResultImage, color_result);
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

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			color_result_map.at<Vec3b>(y, x) = label_colors[ResultLabel.at<uchar>(y, x)];
		}
	}

	TrySetResultMat(this->ResultLabel, color_result_map);
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

	TrySetResultMat(this->ResultImage, composite_image);
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
}

void MontageCore::SolveChannel(int channel_idx, int constraint, const cv::Mat& color_gradient_x, const cv::Mat& color_gradient_y, cv::Mat& output)
{
	int width = color_gradient_x.cols;
	int height = color_gradient_x.rows;
	int NumOfUnknownTerm = 2 * width * height + 1;

	if (solver_type == GradientFusionSolverType::Eigen_Solver)
	{
		Eigen::VectorXd b(NumOfUnknownTerm);
		for (int y = 0; y < height - 1; y++)
		{
			for (int x = 0; x < width - 1; x++)
			{
				int unknown_idx = y * width + x;

				// gradient x
				int eq_idx1 = 2 * unknown_idx;
				Vec3f grads_x = color_gradient_x.at<Vec3f>(y, x);
				b(eq_idx1) = grads_x[channel_idx];

				// gradient y
				int eq_idx2 = 2 * unknown_idx + 1;
				Vec3f grads_y = color_gradient_y.at<Vec3f>(y, x);
				b(eq_idx2) = grads_y[channel_idx];
			}
		}

		int eq_idx = width * height * 2;
		b(eq_idx) = constraint;

		Eigen::VectorXd ATb = A.transpose() * b;

		printf("\nSolving...\n");
		Eigen::ConjugateGradient<Eigen::SparseMatrix<double>> CGSolver(ATA);
		Eigen::VectorXd solution = CGSolver.solve(ATb);

		Eigen::VectorXd solvAX = A * solution;
		TryAppendResultMsg(
			ResultMsg,
			"Constraint of channel " + std::to_string(channel_idx) + " is: " + std::to_string(constraint)
			+ ", and the solved one is: " + std::to_string(solvAX(eq_idx))
		);
		printf("Solved!\n");

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				Vec3b& temp = output.at<Vec3b>(y, x);
				temp[channel_idx] = uchar(std::max(std::min(solution(y * width + x), 255.0), 0.0));
			}
		}
	}
	else if (solver_type == GradientFusionSolverType::My_Solver)
	{
		using namespace std;
		vector<double> b(NumOfUnknownTerm);

		for (int y = 0; y < height - 1; y++)
		{
			for (int x = 0; x < width - 1; x++)
			{
				int idx = y * width + x;
				// gradient X
				int eq_idx = 2 * idx;
				b[eq_idx] = color_gradient_x.at<Vec3f>(y, x)[channel_idx];

				// gradient Y
				eq_idx = 2 * idx + 1;
				b[eq_idx] = color_gradient_y.at<Vec3f>(y, x)[channel_idx];
			}
		}

		b[NumOfUnknownTerm - 1] = constraint;

		Kouek::SparseMat<double> AT = myA.transpose(); // AT m*2m
		vector<double> ATb; // ATb m*1
		AT.multiply(ATb, b);

		vector<double> sol(ATb.size(), 0);
		sol[constraintY * width + constraintX] = constraint; // set constraint
		printf("\nSolving...\n");
		bool ret = Kouek::SparseMat<double>::solveInConjugateGradient(myATA, sol, ATb, 10.0);
		TryAppendResultMsg(
			ResultMsg,
			"Solved. Cov is " + std::to_string(ret)
		);
		printf("Solved. Cov is %d\n", ret);

		for (int y = 0; y < height - 1; y++)
		{
			for (int x = 0; x < width - 1; x++)
			{
				Vec3b& temp = output.at<Vec3b>(y, x);
				temp[channel_idx] = uchar(std::max(std::min(sol[y * width + x], 255.0), 0.0));
			}
		}
		//String fn = "result";
		//fn += std::to_string(channel_idx) + ".png";
		//imwrite(fn, output);
	}
}
