#ifndef IMGPROC_FPGA_H_
#define IMGPROC_FPGA_H_

#include <opencv2/imgproc.hpp>

namespace ecvl {

void ResizeDim_FPGA(const cv::Mat& src, cv::Mat& dst, cv::Size dsize, int interp);
void Threshold_FPGA(const cv::Mat& src,  cv::Mat& dst, double thresh, double maxval);
uint8_t OtsuThreshold_FPGA(const cv::Mat& src);
void warpTransform_FPGA(const cv::Mat& src, cv::Mat& dst, cv::Mat& rotMatrix, cv::Size dsize, int interp);
void GaussianBlur_FPGA(const cv::Mat& src, cv::Mat& dst, float sigma);
void rgb2gray_FPGA(const cv::Mat& src, cv::Mat& dst);

}

#endif // IMGPROC_FPGA_H