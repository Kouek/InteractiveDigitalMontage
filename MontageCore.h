#pragma once
#include <vector>
#include "opencv2/opencv.hpp"

class MontageCore
{
public:
	enum class SmoothTermType
	{
		X,
		X_Plus_Y,
		X_Divide_By_Z
	};
	enum class GradientFusionSolverType
	{
		My_Solver,
		Eigen_Solver
	};
private:
	void BuildSolveMRF(const std::vector<cv::Mat>& Images, const cv::Mat& Label);
	void VisResultLabelMap(const cv::Mat& ResultLabel, int n_label);
	void VisCompositeImage(const cv::Mat& ResultLabel, const std::vector<cv::Mat>& Images);
	void BuildSolveGradientFusion(const std::vector<cv::Mat>& Images, const cv::Mat& ResultLabel);

	void SolveChannel(int channel_idx, int constraint, const cv::Mat& color_gradient_x, const cv::Mat& color_gradient_y, cv::Mat& output);

	void GradientAt(const cv::Mat& Image, int x, int y, cv::Vec3f& grad_x, cv::Vec3f& grad_y);

	std::string* ResultMsg = nullptr;

	cv::Mat* ResultLabel = nullptr;
	cv::Mat* ResultImage = nullptr;
	const std::vector<cv::Vec3b>* ImageColors = nullptr;;
public:
	void RunLabelMatch(const std::vector<cv::Mat>& Images, const cv::Mat& Label,
		double LargePenalty, double SmoothAlpha, SmoothTermType SmoothType);
	void RunGradientFusion(GradientFusionSolverType SolverType);
	void BindResult(std::string* ResultMsg, cv::Mat* ResultLabel, cv::Mat* ResultImage);
	void BindImageColors(const std::vector<cv::Vec3b>* ImageColors);
private:
	cv::flann::Index* AddInertiaConstraint(const cv::Mat& Label);
public:
	enum
	{
		undefined = -1
	};
};