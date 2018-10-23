#include <iostream>
#include <memory>
#include <cassert>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "noise.h"
#include "math2d.h"
#include "utils.h"
#include "perlincontrolfunction.h"
#include "planecontrolfunction.h"
#include "lichtenbergcontrolfunction.h"

using namespace std;

template<typename I>
cv::Mat SegmentImage(const Noise<I>& noise, const Point2D& a, const Point2D&b, int width, int height)
{
	const Point3D point(5.69, -1.34, 4.0);
	const std::array<Segment3D, 2> segments = { {
		{Point3D(1.0, 1.0, 2.0), Point3D(2.0, 3.0, 1.0)},
		{Point3D(2.0, 3.0, 1.0), Point3D(2.0, 5.0, 0.0)}
	} };

	cv::Mat image(height, width, CV_16U);

#pragma omp parallel for shared(image)
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			const double x = remap(double(j), 0.0, double(width), a.x, b.x);
			const double y = remap(double(i), 0.0, double(height), a.y, b.y);

			const double value = noise.displaySegment(x, y, segments, point);

			image.at<uint16_t>(i, j) = uint16_t(value * numeric_limits<uint16_t>::max());
		}
	}

	return image;
}

template<typename I>
vector<vector<double> > EvaluateTerrain(const Noise<I>& noise, const Point2D& a, const Point2D&b, int width, int height)
{
	vector<vector<double> > values(height, vector<double>(width));

#pragma omp parallel for shared(values)
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			const double x = remap_clamp(double(j), 0.0, double(width), a.x, b.x);
			const double y = remap_clamp(double(i), 0.0, double(height), a.y, b.y);

			values[i][j] = noise.evaluateTerrain(x, y);
		}
	}

	return values;
}

template<typename I>
vector<vector<double> > EvaluateLichtenbergFigure(const Noise<I>& noise, const Point2D& a, const Point2D& b, int width, int height)
{
	vector<vector<double> > values(height, vector<double>(width));

#pragma omp parallel for shared(values)
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			const double x = remap_clamp(double(j), 0.0, double(width), a.x, b.x);
			const double y = remap_clamp(double(i), 0.0, double(height), a.y, b.y);

			values[i][j] = noise.evaluateLichtenberg(x, y);
		}
	}

	return values;
}

cv::Mat GenerateImage(const vector<vector<double> > &values)
{
	const int width = int(values.size());
	const int height = int(values.front().size());

	// Find min and max to remap to 16 bits
	double minimum = numeric_limits<double>::max();
	double maximum = numeric_limits<double>::lowest();
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			minimum = min(minimum, values[i][j]);
			maximum = max(maximum, values[i][j]);
		}
	}

	// Convert to 16 bits image
	cv::Mat image(height, width, CV_16U);

#pragma omp parallel for shared(image)
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			const double value = remap_clamp(values[i][j], minimum, maximum, 0.0, 65535.0);

			image.at<uint16_t>(i, j) = uint16_t(value);
		}
	}

	return image;
}

void TerrainImage(int width, int height, const string& filename)
{
	typedef PerlinControlFunction ControlFunctionType;
	unique_ptr<ControlFunctionType> controlFunction(make_unique<ControlFunctionType>());

	const int seed = 0;
	const double eps = 0.15;
	const int resolution = 3;
	const Point2D noiseTopLeft(0.0, 0.0);
	const Point2D noiseBottomRight(4.0, 4.0);
	const Point2D controlFunctionTopLeft(0.0, 0.0);
	const Point2D controlFunctionBottomRight(0.5, 0.5);

	const Noise<ControlFunctionType> noise(move(controlFunction), noiseTopLeft, noiseBottomRight, controlFunctionTopLeft, controlFunctionBottomRight, seed, eps, resolution, false, false, false);
	const cv::Mat image = GenerateImage(EvaluateTerrain(noise, noiseTopLeft, noiseBottomRight, width, height));

	cv::imwrite(filename, image);
}

void LichtenbergFigureImage(int width, int height, const string& filename)
{
	typedef LichtenbergControlFunction ControlFunctionType;
	unique_ptr<ControlFunctionType> controlFunction(make_unique<ControlFunctionType>());

	const int seed = 0;
	const double eps = 0.1;
	const int resolution = 6;
	const Point2D noiseTopLeft(-2.0, -2.0);
	const Point2D noiseBottomRight(2.0, 2.0);
	const Point2D controlFunctionTopLeft(-1.0, -1.0);
	const Point2D controlFunctionBottomRight(1.0, 1.0);

	const Noise<ControlFunctionType> noise(move(controlFunction), noiseTopLeft, noiseBottomRight, controlFunctionTopLeft, controlFunctionBottomRight, seed, eps, resolution, false, true, false);
	const cv::Mat image = GenerateImage(EvaluateLichtenbergFigure(noise, noiseTopLeft, noiseBottomRight, width, height));

	cv::imwrite(filename, image);
}

int main(int argc, char* argv[])
{
	const int WIDTH = 512;
	const int HEIGHT = 512;
	const string FILENAME_TERRAIN = "terrain.png";
	const string FILENAME_LICHTENBERG = "lichtenberg.png";

	TerrainImage(WIDTH, HEIGHT, FILENAME_TERRAIN);
	LichtenbergFigureImage(WIDTH, HEIGHT, FILENAME_LICHTENBERG);

	return 0;
}
