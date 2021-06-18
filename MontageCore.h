#pragma once
#include <vector>
#include "opencv2/opencv.hpp"

class MontageCore
{
public:
	enum class SmoothTermType
	{
		X,
		X_Divide_By_Z
	};
private:
	void BuildSolveMRF(const std::vector<cv::Mat>& Images, const cv::Mat& Label);
	void VisResultLabelMap(const cv::Mat& ResultLabel, int n_label);
	void VisCompositeImage(const cv::Mat& ResultLabel, const std::vector<cv::Mat>& Images);
	void BuildSolveGradientFusion(const std::vector<cv::Mat>& Images, const cv::Mat& ResultLabel);

	void SolveChannel(int channel_idx, int constraint, const cv::Mat& color_gradient_x, const cv::Mat& color_gradient_y, cv::Mat& output);

	void GradientAt(const cv::Mat& Image, int x, int y, cv::Vec3f& grad_x, cv::Vec3f& grad_y);

	std::string* LMResultMsg = nullptr;
	void TryAppendLMResultMsg(const std::string& str);

	cv::Mat* LMResultLabel = nullptr;
	cv::Mat* LMResultImage = nullptr;
	const std::vector<cv::Vec3b>* ImageColors = nullptr;;
public:
	void RunLabel(const std::vector<cv::Mat>& Images, const cv::Mat& Label,
		double LargePenalty, double SmoothAlpha, SmoothTermType SmoothType);
	void BindResult(std::string* Result, cv::Mat* LMResultLabel, cv::Mat* LMResultImage);
	void BindImageColors(const std::vector<cv::Vec3b>* ImageColors);
private:
	cv::flann::Index* AddInertiaConstraint(const cv::Mat& Label);
public:
	enum
	{
		undefined = -1
	};
};