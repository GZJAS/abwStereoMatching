#include "stereo.hpp"
#include "utilities.hpp"

#define DISPARITY_RANGE 40
#define ADAPTIVE_THRESHOLD 32
#define MATCHING_THRESHOLD 8

typedef unsigned int uint;
typedef unsigned char uchar;

cv::Mat generate_disparity_map(cv::Mat image1, cv::Mat image2)
{
	const int kernelSize = 25; // TODO: adaptive kernel size

	preprocess(image1);
	preprocess(image2);

	const uint height = static_cast<uint>(image1.rows);
	const uint width  = static_cast<uint>(image1.cols); // image1 is arbitrary

	cv::Mat output(height, width, CV_8UC1);
	output = cv::Scalar(0);

	uchar* data1 = image1.data;
	uchar* data2 = image2.data;
	cv::Mat::MStep step = image1.step;

	// for each pixel inside the matching region
	for (uint row = (kernelSize / 2) + 1; row < height - (kernelSize / 2) + 1; ++row)
	{
		for (uint col = (kernelSize / 2) + DISPARITY_RANGE; col < width - (kernelSize / 2) + 1; ++col)
		{
			output.at<uchar>(row, col) = get_pixel_disparity(data1, data2, step, row, col, kernelSize);
		}
	}

	postprocess(output);

	return output;
}

void preprocess(cv::Mat& image)
{
	cvtColor(image, image, CV_BGR2Lab, 1);
	cv::resize(image, image, cv::Size(), 0.5f, 0.5f, CV_INTER_LINEAR);
}

void postprocess(cv::Mat& image)
{
	cv::resize(image, image, cv::Size(), 2.0f, 2.0f, CV_INTER_NN);
}

uchar get_pixel_disparity(uchar* data1, uchar* data2, cv::Mat::MStep step, int row, int col, const int& kernelSize)
{
	int pValues[3];
	int qValues[3];

	int difference;



	std::vector<uint> positivesY, positivesX; // containers for adaptive pixel coordinates

	// get_abw_coords(int row, int col,);// for each pixel of bucket
	for (int m = -(kernelSize / 2); m < kernelSize / 2; ++m) // ++m
	{
		for (int n = -(kernelSize / 2); n < kernelSize / 2; ++n) // ++n
		{
			pValues[0] = data1[step * row + col * 3 + 0];
			pValues[1] = data1[step * row + col * 3 + 1];
			pValues[2] = data1[step * row + col * 3 + 2];
			qValues[0] = data1[step * (row + m) + (col + n) * 3 + 0];
			qValues[1] = data1[step * (row + m) + (col + n) * 3 + 1];
			qValues[2] = data1[step * (row + m) + (col + n) * 3 + 2];

			difference = (abs(pValues[0] - qValues[0]) + abs(pValues[1] - qValues[1]) + abs(pValues[2] - qValues[2]));
			// difference = taxicab_dist(pValues[0], pValues[1], pValues[2], qValues[0], qValues[1], qValues[2]);

			if (difference < ADAPTIVE_THRESHOLD)
			{
				positivesY.push_back(row + m);
				positivesX.push_back(col + n);
			}
		}
	}

	std::vector<uint> C(DISPARITY_RANGE + 1, 0); // number of matches for given disparity

	uint numberOfPositives = static_cast<uint>(positivesX.size());

	// TODO: bezveze
	int activeIndex = -1;

	// matching
	for (int disparity = -DISPARITY_RANGE; disparity < 0; ++disparity)
	{
		++activeIndex;
		for (uint pixel = 0; pixel < numberOfPositives; ++pixel)
		{
			pValues[0] = data1[step * positivesY[pixel] + positivesX[pixel] * 3 + 0];
			pValues[1] = data1[step * positivesY[pixel] + positivesX[pixel] * 3 + 1];
			pValues[2] = data1[step * positivesY[pixel] + positivesX[pixel] * 3 + 2];
			qValues[0] = data2[step * positivesY[pixel] + (positivesX[pixel] + disparity) * 3 + 0];
			qValues[1] = data2[step * positivesY[pixel] + (positivesX[pixel] + disparity) * 3 + 1];
			qValues[2] = data2[step * positivesY[pixel] + (positivesX[pixel] + disparity) * 3 + 2];

			difference = (abs(pValues[0] - qValues[0]) + abs(pValues[1] - qValues[1]) + abs(pValues[2] - qValues[2]));
			// difference = taxicab_dist(pValues[0], pValues[1], pValues[2], qValues[0], qValues[1], qValues[2]);

			if (difference < MATCHING_THRESHOLD)
			{
				C[activeIndex] = C[activeIndex] + 1;
			}
		}
	}

	uint maximum = *std::max_element(C.begin(), C.end());
	int ambiguous = 0;
	for (int a = 0; a < C.size(); ++a)
	{
		if (C[a] == maximum) { ambiguous += 1; }
	}

	// TODO: find index of maximum (avoid for loop)
	for (uint index = 0; index < C.size(); ++index)
	{
		if (C[index] == maximum /*&& ambiguous == 1*/)
		{
			return (DISPARITY_RANGE - index) * 255 / (DISPARITY_RANGE + 1);
			break;
		}
	}
}
