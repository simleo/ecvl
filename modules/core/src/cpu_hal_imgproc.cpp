/*
* ECVL - European Computer Vision Library
* Version: 0.1
* copyright (c) 2020, Universit� degli Studi di Modena e Reggio Emilia (UNIMORE), AImageLab
* Authors:
*    Costantino Grana (costantino.grana@unimore.it)
*    Federico Bolelli (federico.bolelli@unimore.it)
*    Michele Cancilla (michele.cancilla@unimore.it)
*    Laura Canalini (laura.canalini@unimore.it)
*    Stefano Allegretti (stefano.allegretti@unimore.it)
* All rights reserved.
*/

#include "ecvl/core/cpu_hal.h"

#include <random>
#include <opencv2/photo.hpp>

#include "ecvl/core/imgproc.h"
#include "ecvl/core/arithmetic.h"
#include "ecvl/core/support_opencv.h"

using namespace std;

namespace ecvl
{
inline void RGB2GRAYGeneric(const uint8_t* r, const uint8_t* g, const uint8_t* b, uint8_t* dst, DataType dt)
{
    // 0.299 * R + 0.587 * G + 0.114 * B

#define DEREF(ptr, type)            *reinterpret_cast<TypeInfo_t<DataType::type>*>(ptr)
#define CONST_DEREF(ptr, type)      *reinterpret_cast<const TypeInfo_t<DataType::type>*>(ptr)

#define ECVL_TUPLE(type, ...) \
case DataType::type: DEREF(dst, type) = saturate_cast<DataType::type>(0.299 * CONST_DEREF(r, type) + 0.587 * CONST_DEREF(g, type) + 0.114 * CONST_DEREF(b, type));  break;

    switch (dt) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
    }

#undef ECVL_TUPLE
#undef DEREF
#undef CONST_DEREF
}

void OpenCVAlwaysCheck(const ecvl::Image& src)
{
    if (!(src.Width() && src.Height() && src.Channels() && vsize(src.dims_) == 3 && src.elemtype_ != DataType::int64)) {
        ECVL_ERROR_NOT_IMPLEMENTED
    }
}

void CpuHal::ResizeDim(const ecvl::Image& src, ecvl::Image& dst, const std::vector<int>& newdims, InterpolationType interp)
{
    OpenCVAlwaysCheck(src);

    cv::Mat m;
    cv::resize(ImageToMat(src), m, cv::Size(newdims[0], newdims[1]), 0.0, 0.0, GetOpenCVInterpolation(interp));
    dst = ecvl::MatToImage(m);
}

void CpuHal::ResizeScale(const Image& src, Image& dst, const std::vector<double>& scales, InterpolationType interp)
{
    OpenCVAlwaysCheck(src);

    int nw = lround(src.dims_[0] * scales[0]);
    int nh = lround(src.dims_[1] * scales[1]);

    cv::Mat m;
    cv::resize(ImageToMat(src), m, cv::Size(nw, nh), 0.0, 0.0, GetOpenCVInterpolation(interp));
    dst = ecvl::MatToImage(m);
}

void CpuHal::Flip2D(const ecvl::Image& src, ecvl::Image& dst)
{
    size_t c_pos = src.channels_.find('c');
    size_t x_pos = src.channels_.find('x');
    size_t y_pos = src.channels_.find('y');

    if (c_pos == string::npos || x_pos == string::npos || y_pos == string::npos) {
        ECVL_ERROR_WRONG_PARAMS("Malformed src image")
    }

    int src_width = src.Width();
    int src_height = src.Height();
    int src_channels = src.Channels();
    Image tmp(src.dims_, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    int src_stride_c = src.strides_[c_pos];
    int src_stride_x = src.strides_[x_pos];
    int src_stride_y = src.strides_[y_pos];

    vector<uint8_t*> src_vch(src_channels), tmp_vch(src_channels);

    // Get the pointers to channels starting pixels
    for (int i = 0; i < src_channels; ++i) {
        src_vch[i] = src.data_ + i * src_stride_c;
        tmp_vch[i] = tmp.data_ + i * src_stride_c;
    }

    int pivot = (src_height + 1) / 2;
    for (int r = 0, r_end = src_height - 1; r < pivot; ++r, --r_end) {
        // Get the address of next row
        int r1 = r * src_stride_y;
        int r2 = r_end * src_stride_y;
        for (int c = 0; c < src_width; ++c) {
            // Get the address of pixels in current row
            int p1 = r1 + src_stride_x * c;
            int p2 = r2 + src_stride_x * c;

#define ECVL_TUPLE(type, ...) \
        case DataType::type: \
            for (int ch = 0; ch < src_channels; ++ch) { \
                *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p1) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch] + p2); \
                *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p2) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch] + p1); \
            } \
            break;

            switch (src.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
            }

#undef ECVL_TUPLE
        }
    }
    dst = std::move(tmp);
}

void CpuHal::Mirror2D(const ecvl::Image& src, ecvl::Image& dst)
{
    size_t c_pos = src.channels_.find('c');
    size_t x_pos = src.channels_.find('x');
    size_t y_pos = src.channels_.find('y');

    if (c_pos == string::npos || x_pos == string::npos || y_pos == string::npos) {
        ECVL_ERROR_WRONG_PARAMS("Malformed src image")
    }

    int src_width = src.Width();
    int src_height = src.Height();
    int src_channels = src.Channels();
    Image tmp(src.dims_, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    int src_stride_c = src.strides_[c_pos];
    int src_stride_x = src.strides_[x_pos];
    int src_stride_y = src.strides_[y_pos];
    vector<uint8_t*> src_vch(src_channels), tmp_vch(src_channels);

    // Get the pointers to channels starting pixels
    for (int i = 0; i < src_channels; ++i) {
        src_vch[i] = src.data_ + i * src_stride_c;
        tmp_vch[i] = tmp.data_ + i * src_stride_c;
    }

    int pivot = (src_width + 1) / 2;
    for (int r = 0; r < src_height; ++r) {
        // Get the address of next row
        int pos = r * src_stride_y;
        for (int c = 0, c_end = src_width - 1; c < pivot; ++c, --c_end) {
            // Get the address of pixels in current row
            int p1 = pos + src_stride_x * c;
            int p2 = pos + src_stride_x * c_end;

#define ECVL_TUPLE(type, ...) \
        case DataType::type: \
            for (int ch = 0; ch < src_channels; ++ch) { \
                *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p1) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch] + p2); \
                *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p2) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch] + p1); \
            } \
            break;

            switch (src.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
            }

#undef ECVL_TUPLE
        }
    }
    dst = std::move(tmp);
}

void CpuHal::Rotate2D(const ecvl::Image& src, ecvl::Image& dst, double angle, const std::vector<double>& center, double scale, InterpolationType interp)
{
    OpenCVAlwaysCheck(src);

    cv::Point2f pt;
    if (center.empty()) {
        pt = { src.dims_[0] / 2.0f, src.dims_[1] / 2.0f };
    }
    else {
        pt = { float(center[0]), float(center[1]) };
    }

    cv::Mat rot_matrix;
    rot_matrix = cv::getRotationMatrix2D(pt, -angle, scale);
    cv::Mat m;
    cv::warpAffine(ImageToMat(src), m, rot_matrix, { src.dims_[0], src.dims_[1] }, GetOpenCVInterpolation(interp));
    dst = ecvl::MatToImage(m);
}

void CpuHal::RotateFullImage2D(const ecvl::Image& src, ecvl::Image& dst, double angle, double scale, InterpolationType interp)
{
    OpenCVAlwaysCheck(src);

    cv::Point2f pt;
    pt = { src.dims_[0] / 2.0f, src.dims_[1] / 2.0f };

    cv::Mat1d rot_matrix;
    rot_matrix = cv::getRotationMatrix2D(pt, -angle, scale);

    int w = src.dims_[0];
    int h = src.dims_[1];

    double cos = abs(rot_matrix(0, 0));
    double sin = abs(rot_matrix(0, 1));

    int nw = lround((h * sin) + (w * cos));
    int nh = lround((h * cos) + (w * sin));

    rot_matrix(0, 2) += (nw / 2) - pt.x;
    rot_matrix(1, 2) += (nh / 2) - pt.y;

    cv::Mat m;
    cv::warpAffine(ImageToMat(src), m, rot_matrix, { nw, nh }, GetOpenCVInterpolation(interp));
    dst = ecvl::MatToImage(m);
}

void CpuHal::ChangeColorSpace(const Image& src, Image& dst, ColorType new_type)
{
    if (src.colortype_ == ColorType::HSV || new_type == ColorType::HSV
        ||
        src.colortype_ == ColorType::YCbCr || new_type == ColorType::YCbCr
        ||
        src.colortype_ == ColorType::RGBA || new_type == ColorType::RGBA
        ) {
        ECVL_ERROR_NOT_IMPLEMENTED
    }

    Image tmp;

    if (src.colortype_ == ColorType::GRAY) {
        if (new_type == ColorType::RGB || new_type == ColorType::BGR) {
            if (src.channels_ == "xyc") {
                auto dims = src.dims_;
                dims[2] = 3;
                tmp.Create(dims, src.elemtype_, "xyc", new_type, src.spacings_, src.dev_);
                auto plane0 = tmp.data_ + 0 * tmp.strides_[2];
                auto plane1 = tmp.data_ + 1 * tmp.strides_[2];
                auto plane2 = tmp.data_ + 2 * tmp.strides_[2];
                if (src.contiguous_) {
                    memcpy(plane0, src.data_, src.datasize_);
                    memcpy(plane1, src.data_, src.datasize_);
                    memcpy(plane2, src.data_, src.datasize_);
                }
                else {
                    auto i = src.Begin<uint8_t>(), e = src.End<uint8_t>();
                    for (; i != e; ++i) {
                        memcpy(plane0, i.ptr_, src.elemsize_);
                        memcpy(plane1, i.ptr_, src.elemsize_);
                        memcpy(plane2, i.ptr_, src.elemsize_);
                        plane0 += src.elemsize_;
                        plane1 += src.elemsize_;
                        plane2 += src.elemsize_;
                    }
                }
            }
            else if (src.channels_ == "cxy") {
                auto dims = src.dims_;
                dims[0] = 3;
                tmp.Create(dims, src.elemtype_, "cxy", new_type, src.spacings_, src.dev_);
                auto plane0 = tmp.data_ + 0 * tmp.strides_[0];
                auto plane1 = tmp.data_ + 1 * tmp.strides_[0];
                auto plane2 = tmp.data_ + 2 * tmp.strides_[0];
                auto i = src.Begin<uint8_t>(), e = src.End<uint8_t>();
                for (; i != e; ++i) {
                    memcpy(plane0, i.ptr_, src.elemsize_);
                    memcpy(plane1, i.ptr_, src.elemsize_);
                    memcpy(plane2, i.ptr_, src.elemsize_);
                    plane0 += 3 * src.elemsize_;
                    plane1 += 3 * src.elemsize_;
                    plane2 += 3 * src.elemsize_;
                }
            }
            else {
                ECVL_ERROR_NOT_IMPLEMENTED
            }
        }
        dst = std::move(tmp);
        return;
    }

    if ((src.colortype_ == ColorType::RGB || src.colortype_ == ColorType::BGR) && new_type == ColorType::GRAY) {
        size_t c_pos = src.channels_.find('c');
        if (c_pos == std::string::npos) {
            ECVL_ERROR_WRONG_PARAMS("Malformed src image")
        }

        std::vector<int> tmp_dims = src.dims_;
        tmp_dims[c_pos] = 1;

        tmp.Create(tmp_dims, src.elemtype_, src.channels_, ColorType::GRAY, src.spacings_);

        const uint8_t* r = src.data_ + ((src.colortype_ == ColorType::RGB) ? 0 : 2) * src.strides_[c_pos];
        const uint8_t* g = src.data_ + 1 * src.strides_[c_pos];
        const uint8_t* b = src.data_ + ((src.colortype_ == ColorType::RGB) ? 2 : 0) * src.strides_[c_pos];

        for (size_t tmp_pos = 0; tmp_pos < tmp.datasize_; tmp_pos += tmp.elemsize_) {
            int x = static_cast<int>(tmp_pos);
            int src_pos = 0;
            for (int i = vsize(tmp.dims_) - 1; i >= 0; i--) {
                if (i != c_pos) {
                    src_pos += (x / tmp.strides_[i]) * src.strides_[i];
                    x %= tmp.strides_[i];
                }
            }

            RGB2GRAYGeneric(r + src_pos, g + src_pos, b + src_pos, tmp.data_ + tmp_pos, src.elemtype_);
        }
        dst = std::move(tmp);
        return;
    }

    //TODO: update with the operator+ for iterators
    if (src.colortype_ == ColorType::BGR && new_type == ColorType::RGB
        ||
        src.colortype_ == ColorType::RGB && new_type == ColorType::BGR) {
        if (src.channels_ == "xyc") {
            tmp.Create(src.dims_, src.elemtype_, "xyc", new_type, src.spacings_, src.dev_);
            auto plane0 = tmp.data_ + 0 * tmp.strides_[2];
            auto plane1 = tmp.data_ + 1 * tmp.strides_[2];
            auto plane2 = tmp.data_ + 2 * tmp.strides_[2];
            if (src.contiguous_) {
                memcpy(plane0, src.data_ + 2 * src.strides_[2], src.strides_[2]);
                memcpy(plane1, src.data_ + 1 * src.strides_[2], src.strides_[2]);
                memcpy(plane2, src.data_ + 0 * src.strides_[2], src.strides_[2]);
            }
            else {
                ECVL_ERROR_NOT_IMPLEMENTED
            }
        }
        else if (src.channels_ == "cxy") {
            tmp.Create(src.dims_, src.elemtype_, "cxy", new_type, src.spacings_, src.dev_);
            auto plane0 = tmp.data_ + 0 * tmp.strides_[0];
            auto plane1 = tmp.data_ + 1 * tmp.strides_[0];
            auto plane2 = tmp.data_ + 2 * tmp.strides_[0];
            auto i = src.Begin<uint8_t>(), e = src.End<uint8_t>();
            for (; i != e; ++i) {
                memcpy(plane0, i.ptr_ + 2 * src.elemsize_, src.elemsize_);
                memcpy(plane1, i.ptr_ + 1 * src.elemsize_, src.elemsize_);
                memcpy(plane2, i.ptr_ + 0 * src.elemsize_, src.elemsize_);
                plane0 += 3 * src.elemsize_;
                plane1 += 3 * src.elemsize_;
                plane2 += 3 * src.elemsize_;
                ++i; ++i;
            }
        }
        else {
            ECVL_ERROR_NOT_IMPLEMENTED
        }
        dst = std::move(tmp);
        return;
    }

    ECVL_ERROR_NOT_REACHABLE_CODE
}

template <typename T>
void ThresholdImpl(const Image& src, Image& dst, double thresh, double maxval, ThresholdingType thresh_type)
{
    Image tmp(src.dims_, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    auto thresh_t = saturate_cast<T>(thresh);
    auto maxval_t = saturate_cast<T>(maxval);
    auto minval_t = static_cast<T>(0);
    T* src_data = reinterpret_cast<T*>(src.data_);
    T* tmp_data = reinterpret_cast<T*>(tmp.data_);
    auto elemsize = src.elemsize_;

    switch (thresh_type) {
    case ecvl::ThresholdingType::BINARY:
        for (size_t i = 0; i < tmp.datasize_; i += elemsize) {
            *tmp_data = *src_data > thresh_t ? maxval_t : minval_t;
            ++src_data;
            ++tmp_data;
        }
        break;
    case ecvl::ThresholdingType::BINARY_INV:
        for (size_t i = 0; i < tmp.datasize_; i += elemsize) {
            *tmp_data = *src_data <= thresh_t ? maxval_t : minval_t;
            ++src_data;
            ++tmp_data;
        }
        break;
    }
    dst = std::move(tmp);
}

void CpuHal::Threshold(const Image& src, Image& dst, double thresh, double maxval, ThresholdingType thresh_type)
{
#define ECVL_TUPLE(type, ...) \
case DataType::type: ThresholdImpl<TypeInfo_t<DataType::type>>(src, dst, thresh, maxval, thresh_type); break;

    switch (src.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
    }

#undef ECVL_TUPLE
}

std::vector<double> CpuHal::Histogram(const Image& src)
{
    std::vector<double> hist(256, 0);

    ConstView<DataType::uint8> view(src);
    for (auto it = view.Begin(); it != view.End(); ++it) {
        hist[*it]++;
    }

    int total_pixels = std::accumulate(src.dims_.begin(), src.dims_.end(), 1, std::multiplies<int>());
    for (auto it = hist.begin(); it < hist.end(); it++) {
        *it /= total_pixels;
    }

    return hist;
}

int CpuHal::OtsuThreshold(const Image& src)
{
    std::vector<double> hist = Histogram(src);

    double mu_t = 0;
    for (size_t i = 1; i < hist.size(); i++) {
        mu_t += hist[i] * i;
    }

    double w_k = 0;
    double mu_k = 0;
    double sigma_max = 0;
    int threshold = 0;
    int hsize = vsize(hist);
    for (int k = 0; k < hsize - 1; k++) {
        w_k += hist[k];
        mu_k += hist[k] * k;

        double sigma = ((mu_t * w_k - mu_k) * (mu_t * w_k - mu_k)) / (w_k * (1 - w_k));
        if (sigma > sigma_max) {
            sigma_max = sigma;
            threshold = k;
        }
    }

    return threshold;
}

void CpuHal::Filter2D(const Image& src, Image& dst, const Image& ker, DataType type)
{
    Image tmp(src.dims_, type, src.channels_, src.colortype_, src.spacings_, src.dev_);

    int hlf_width = ker.dims_[0] / 2;
    int hlf_height = ker.dims_[1] / 2;

    TypeInfo_t<DataType::float64>* ker_data = reinterpret_cast<TypeInfo_t<DataType::float64>*>(ker.data_);

    uint8_t* tmp_ptr = tmp.data_;

    //auto tmp_it = tmp.ContiguousBegin<TypeInfo_t<DataType::uint8>>();
    //auto tmp_it_end = tmp.ContiguousEnd<TypeInfo_t<DataType::uint8>>();

    TypeInfo_t<DataType::uint8>* src_data = reinterpret_cast<TypeInfo_t<DataType::uint8>*>(src.data_);
    for (int chan = 0; chan < tmp.dims_[2]; chan++) {
        for (int r = 0; r < tmp.dims_[1]; r++) {
            for (int c = 0; c < tmp.dims_[0]; c++) {
                double acc = 0;
                int i = 0;
                for (int rk = 0; rk < ker.dims_[1]; rk++) {
                    for (int ck = 0; ck < ker.dims_[0]; ck++) {
                        int x = c + ck - hlf_width;
                        if (x < 0) x = 0; else if (x >= tmp.dims_[0]) x = tmp.dims_[0] - 1;

                        int y = r + rk - hlf_height;
                        if (y < 0) y = 0; else if (y >= tmp.dims_[1]) y = tmp.dims_[1] - 1;

                        acc += ker_data[i] * src_data[x + y * src.strides_[1]];

                        i++;
                    }
                }

#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_ptr) = static_cast<TypeInfo_t<DataType::type>>(acc); break;

                switch (type) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
                }

#undef ECVL_TUPLE

                tmp_ptr += tmp.elemsize_;
            }
        }

        src_data += src.strides_[2] / sizeof(*src_data);
    }
    dst = std::move(tmp);
}

void CpuHal::SeparableFilter2D(const Image& src, Image& dst, const vector<double>& kerX, const vector<double>& kerY, DataType type)
{
    Image tmp1(src.dims_, DataType::float64, src.channels_, src.colortype_, src.spacings_);
    Image tmp2(src.dims_, type, src.channels_, src.colortype_, src.spacings_, src.dev_);
    int hlf_width = vsize(kerX) / 2;
    int hlf_height = vsize(kerY) / 2;

    // X direction
    auto tmp1_it = tmp1.ContiguousBegin<TypeInfo_t<DataType::float64>>();
#define ECVL_TUPLE(type, ...) \
case DataType::type: \
    { \
        TypeInfo_t<DataType::type>* src_data = reinterpret_cast<TypeInfo_t<DataType::type>*>(src.data_); \
        for (int chan = 0; chan < tmp1.dims_[2]; chan++) { \
            for (int r = 0; r < tmp1.dims_[1]; r++) { \
                for (int c = 0; c < tmp1.dims_[0]; c++) { \
                    double acc = 0; \
                    for (unsigned int ck = 0; ck < kerX.size(); ck++) { \
                        int x = c + ck - hlf_width; \
                        if (x < 0) x = 0; else if (x >= tmp1.dims_[0]) x = tmp1.dims_[0] - 1; \
                        acc += kerX[ck] * src_data[x]; \
                    } \
                *tmp1_it = acc; \
                ++tmp1_it; \
                } \
                src_data += src.strides_[1] / sizeof(*src_data); \
            } \
        } \
    } \
    break;

    switch (type) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
    }

#undef ECVL_TUPLE

    uint8_t* tmp2_ptr = tmp2.data_;

    // Y direction
    TypeInfo_t<DataType::float64>* tmp1_data = reinterpret_cast<TypeInfo_t<DataType::float64>*>(tmp1.data_);
    for (int chan = 0; chan < tmp2.dims_[2]; chan++) {
        for (int r = 0; r < tmp2.dims_[1]; r++) {
            for (int c = 0; c < tmp2.dims_[0]; c++) {
                double acc = 0;
                for (unsigned int rk = 0; rk < kerY.size(); rk++) {
                    int y = r + rk - hlf_height;
                    if (y < 0) y = 0; else if (y >= tmp2.dims_[1]) y = tmp2.dims_[1] - 1;

                    acc += kerY[rk] * tmp1_data[c + y * tmp1.strides_[1] / sizeof(*tmp1_data)];
                }

#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp2_ptr) = static_cast<TypeInfo_t<DataType::type>>(acc); break;

                switch (type) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
                }

#undef ECVL_TUPLE

                tmp2_ptr += tmp2.elemsize_;
            }
        }

        tmp1_data += tmp1.strides_[2] / sizeof(*tmp1_data);
    }
    dst = std::move(tmp2);
}

void CpuHal::GaussianBlur(const Image& src, Image& dst, int sizeX, int sizeY, double sigmaX, double sigmaY)
{
    // Find x kernel values
    vector<double> kernelX(sizeX);
    double sum = 0;
    for (int i = 0; i < sizeX; i++) {
        double coeff = exp(-((i - (sizeX - 1) / 2) * (i - (sizeX - 1) / 2)) / (2 * sigmaX * sigmaX));
        sum += coeff;
        kernelX[i] = coeff;
    }
    for (int i = 0; i < sizeX; i++) {
        kernelX[i] /= sum;
    }

    // Find y kernel values
    vector<double> kernelY(sizeY);
    sum = 0;
    for (int i = 0; i < sizeY; i++) {
        double coeff = exp(-((i - (sizeY - 1) / 2) * (i - (sizeY - 1) / 2)) / (2 * sigmaY * sigmaY));
        sum += coeff;
        kernelY[i] = coeff;
    }
    for (int i = 0; i < sizeY; i++) {
        kernelY[i] /= sum;
    }

    ecvl::SeparableFilter2D(src, dst, kernelX, kernelY);
}

void CpuHal::AdditiveLaplaceNoise(const Image& src, Image& dst, double std_dev)
{
    Image tmp(src.dims_, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    random_device rd;
    mt19937 gen(rd());
    exponential_distribution<> dist(1 / std_dev);

    for (uint8_t* tmp_ptr = tmp.data_, *src_ptr = src.data_; tmp_ptr < tmp.data_ + tmp.datasize_; tmp_ptr += tmp.elemsize_, src_ptr += src.elemsize_) {
        double exp1 = dist(gen);
        double exp2 = dist(gen);

        double noise = exp1 - exp2;

#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_ptr) = saturate_cast<TypeInfo_t<DataType::type>>(noise + *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_ptr)); break;

        switch (tmp.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
        }

#undef ECVL_TUPLE
    }
    dst = std::move(tmp);
}

void CpuHal::AdditivePoissonNoise(const Image& src, Image& dst, double lambda)
{
    Image tmp(src.dims_, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    random_device rd;
    mt19937 gen(rd());
    poisson_distribution<> dist(lambda);

    for (uint8_t* tmp_ptr = tmp.data_, *src_ptr = src.data_; tmp_ptr < tmp.data_ + tmp.datasize_; tmp_ptr += tmp.elemsize_, src_ptr += src.elemsize_) {
        double noise = dist(gen);

#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_ptr) = saturate_cast<TypeInfo_t<DataType::type>>(noise + *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_ptr)); break;

        switch (tmp.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
        }

#undef ECVL_TUPLE
    }
    dst = std::move(tmp);
}

void CpuHal::GammaContrast(const Image& src, Image& dst, double gamma)
{
    Image tmp(src.dims_, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    for (uint8_t* tmp_ptr = tmp.data_, *src_ptr = src.data_; tmp_ptr < tmp.data_ + tmp.datasize_; tmp_ptr += tmp.elemsize_, src_ptr += src.elemsize_) {
#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_ptr) = saturate_cast<TypeInfo_t<DataType::type>>(pow(*reinterpret_cast<TypeInfo_t<DataType::type>*>(src_ptr) / 255., gamma) * 255); break;

        switch (tmp.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
        }

#undef ECVL_TUPLE
    }
    dst = std::move(tmp);
}

void CpuHal::CoarseDropout(const Image& src, Image& dst, double p, double drop_size, bool per_channel)
{
    Image tmp = src;

    int rectX = static_cast<int>(src.dims_[0] * drop_size);
    int rectY = static_cast<int>(src.dims_[1] * drop_size);

    if (rectX == 0) {
        ++rectX;
    }
    if (rectY == 0) {
        ++rectY;
    }

    random_device rd;
    mt19937 gen(rd());
    discrete_distribution<> dist({ p, 1 - p });

    if (per_channel) {
        for (int ch = 0; ch < src.dims_[2]; ch++) {
            uint8_t* tmp_ptr = tmp.Ptr({ 0, 0, ch });

            for (int r = 0; r < src.dims_[1]; r += rectY) {
                for (int c = 0; c < src.dims_[0]; c += rectX) {
                    if (dist(gen) == 0) {
                        for (int rdrop = r; rdrop < r + rectY && rdrop < src.dims_[1]; rdrop++) {
                            for (int cdrop = c; cdrop < c + rectX && cdrop < src.dims_[0]; cdrop++) {
#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_ptr + rdrop * tmp.strides_[1] + cdrop * tmp.strides_[0]) = static_cast<TypeInfo_t<DataType::type>>(0); break;

                                switch (tmp.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
                                }

#undef ECVL_TUPLE
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        vector<uint8_t*> channel_ptrs;
        for (int ch = 0; ch < tmp.dims_[2]; ch++) {
            channel_ptrs.push_back(tmp.Ptr({ 0, 0, ch }));
        }

        for (int r = 0; r < src.dims_[1]; r += rectY) {
            for (int c = 0; c < src.dims_[0]; c += rectX) {
                if (dist(gen) == 0) {
                    for (int ch = 0; ch < src.dims_[2]; ch++) {
                        for (int rdrop = r; rdrop < r + rectY && rdrop < src.dims_[1]; rdrop++) {
                            for (int cdrop = c; cdrop < c + rectX && cdrop < src.dims_[0]; cdrop++) {
#define ECVL_TUPLE(type, ...) \
case DataType::type: *reinterpret_cast<TypeInfo_t<DataType::type>*>(channel_ptrs[ch] + rdrop * tmp.strides_[1] + cdrop * tmp.strides_[0]) = static_cast<TypeInfo_t<DataType::type>>(0); break;

                                switch (tmp.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
                                }

#undef ECVL_TUPLE
                            }
                        }
                    }
                }
            }
        }
    }
    dst = std::move(tmp);
}

void CpuHal::IntegralImage(const Image& src, Image& dst, DataType dst_type)
{
    Image tmp({ src.dims_[0] + 1, src.dims_[1] + 1, src.dims_[2] }, dst_type, src.channels_, ColorType::GRAY, src.spacings_, src.dev_);

    ConstContiguousViewXYC<DataType::uint8> vsrc(src);
    ContiguousViewXYC<DataType::float64> vtmp(tmp);

    switch (tmp.elemtype_) {
    case DataType::float64:
        for (int y = 0; y < vtmp.height(); ++y) {
            for (int x = 0; x < vtmp.width(); ++x) {
                if (!x || !y) {
                    vtmp(x, y, 0) = 0.;
                }
                else {
                    vtmp(x, y, 0) = vsrc(x - 1, y - 1, 0) + vtmp(x - 1, y, 0) + vtmp(x, y - 1, 0) - vtmp(x - 1, y - 1, 0);
                }
            }
        }
        break;
    }
    dst = std::move(tmp);
}

void CpuHal::NonMaximaSuppression(const Image& src, Image& dst)
{
    Image tmp = src;
    memset(tmp.data_, 0, tmp.datasize_);

    ConstContiguousViewXYC<DataType::int32> vsrc(src);
    ContiguousViewXYC<DataType::int32> vtmp(tmp);

    for (int y = 1; y < vtmp.height() - 1; ++y) {
        for (int x = 1; x < vtmp.width() - 1; ++x) {
            int cur = vsrc(x, y, 0);
            if (cur < vsrc(x - 1, y - 1, 0) ||
                cur < vsrc(x - 1, y, 0) ||
                cur < vsrc(x - 1, y + 1, 0) ||
                cur < vsrc(x, y - 1, 0) ||
                cur < vsrc(x, y + 1, 0) ||
                cur < vsrc(x + 1, y - 1, 0) ||
                cur < vsrc(x + 1, y, 0) ||
                cur < vsrc(x + 1, y + 1, 0)) {
                continue;
            }
            vtmp(x, y, 0) = cur;
        }
    }
    dst = std::move(tmp);
}

vector<ecvl::Point2i> CpuHal::GetMaxN(const Image& src, size_t n)
{
    ConstContiguousViewXYC<DataType::int32> vsrc(src);
    using pqt = pair<int32_t, Point2i>;
    vector<pqt> pq(n, make_pair(std::numeric_limits<int32_t>::min(), Point2i{ -1,-1 }));
    for (int y = 0; y < vsrc.height(); ++y) { // rows
        for (int x = 0; x < vsrc.width(); ++x) { // cols
            int32_t cur_value = vsrc(x, y, 0);
            if (cur_value > pq.front().first) {
                pop_heap(begin(pq), end(pq), greater<pqt>{});
                pq.back() = make_pair(cur_value, Point2i{ x,y });
                push_heap(begin(pq), end(pq), greater<pqt>{});
            }
        }
    }

    vector<ecvl::Point2i> max_coords;
    max_coords.reserve(n);
    for (const auto& x : pq) {
        max_coords.push_back(x.second);
    }
    return max_coords;
}

// Union-Find (UF) with path compression (PC) as in:
// Two Strategies to Speed up Connected Component Labeling Algorithms
// Kesheng Wu, Ekow Otoo, Kenji Suzuki
struct UFPC
{
    // Maximum number of labels (included background) = 2^(sizeof(unsigned) x 8)
    unsigned* P_;
    unsigned length_;

    unsigned NewLabel()
    {
        P_[length_] = length_;
        return length_++;
    }

    unsigned Merge(unsigned i, unsigned j)
    {
        // FindRoot(i)
        unsigned root(i);
        while (P_[root] < root) {
            root = P_[root];
        }
        if (i != j) {
            // FindRoot(j)
            unsigned root_j(j);
            while (P_[root_j] < root_j) {
                root_j = P_[root_j];
            }
            if (root > root_j) {
                root = root_j;
            }
            // SetRoot(j, root);
            while (P_[j] < j) {
                unsigned t = P_[j];
                P_[j] = root;
                j = t;
            }
            P_[j] = root;
        }
        // SetRoot(i, root);
        while (P_[i] < i) {
            unsigned t = P_[i];
            P_[i] = root;
            i = t;
        }
        P_[i] = root;
        return root;
    }
};

void CpuHal::ConnectedComponentsLabeling(const Image& src, Image& dst)
{
    Image tmp(src.dims_, DataType::int32, "xyc", ColorType::GRAY, src.spacings_, src.dev_);

    const int h = src.dims_[1];
    const int w = src.dims_[0];

    UFPC ufpc;
    unsigned*& P = ufpc.P_;
    unsigned& P_length = ufpc.length_;

    P = new unsigned[((size_t)((h + 1) / 2) * (size_t)((w + 1) / 2) + 1)];
    P[0] = 0; // First label is for background pixels
    P_length = 1;

    int e_rows = h & 0xfffffffe;
    bool o_rows = h % 2 == 1;
    int e_cols = w & 0xfffffffe;
    bool o_cols = w % 2 == 1;

    // We work with 2x2 blocks
    // +-+-+-+
    // |P|Q|R|
    // +-+-+-+
    // |S|X|
    // +-+-+

    // The pixels are named as follows
    // +---+---+---+
    // |a b|c d|e f|
    // |g h|i j|k l|
    // +---+---+---+
    // |m n|o p|
    // |q r|s t|
    // +---+---+

    // Pixels a, f, l, q are not needed, since we need to understand the
    // the connectivity between these blocks and those pixels only matter
    // when considering the outer connectivities

    // A bunch of defines used to check if the pixels are foreground,
    // without going outside the image limits.

    // First scan

// Define Conditions and Actions
    {
#define CONDITION_B img_row_prev_prev[c-1]>0
#define CONDITION_C img_row_prev_prev[c]>0
#define CONDITION_D img_row_prev_prev[c+1]>0
#define CONDITION_E img_row_prev_prev[c+2]>0

#define CONDITION_G img_row_prev[c-2]>0
#define CONDITION_H img_row_prev[c-1]>0
#define CONDITION_I img_row_prev[c]>0
#define CONDITION_J img_row_prev[c+1]>0
#define CONDITION_K img_row_prev[c+2]>0

#define CONDITION_M img_row[c-2]>0
#define CONDITION_N img_row[c-1]>0
#define CONDITION_O img_row[c]>0
#define CONDITION_P img_row[c+1]>0

#define CONDITION_R img_row_fol[c-1]>0
#define CONDITION_S img_row_fol[c]>0
#define CONDITION_T img_row_fol[c+1]>0

        // Action 1: No action
#define ACTION_1 img_labels_row[c] = 0;
                               // Action 2: New label (the block has foreground pixels and is not connected to anything else)
#define ACTION_2 img_labels_row[c] = ufpc.NewLabel();
                               //Action 3: Assign label of block P
#define ACTION_3 img_labels_row[c] = img_labels_row_prev_prev[c - 2];
                               // Action 4: Assign label of block Q
#define ACTION_4 img_labels_row[c] = img_labels_row_prev_prev[c];
                               // Action 5: Assign label of block R
#define ACTION_5 img_labels_row[c] = img_labels_row_prev_prev[c + 2];
                               // Action 6: Assign label of block S
#define ACTION_6 img_labels_row[c] = img_labels_row[c - 2];
                               // Action 7: Merge labels of block P and Q
#define ACTION_7 img_labels_row[c] = ufpc.Merge(img_labels_row_prev_prev[c - 2], img_labels_row_prev_prev[c]);
                               //Action 8: Merge labels of block P and R
#define ACTION_8 img_labels_row[c] = ufpc.Merge(img_labels_row_prev_prev[c - 2], img_labels_row_prev_prev[c + 2]);
                               // Action 9 Merge labels of block P and S
#define ACTION_9 img_labels_row[c] = ufpc.Merge(img_labels_row_prev_prev[c - 2], img_labels_row[c - 2]);
                               // Action 10 Merge labels of block Q and R
#define ACTION_10 img_labels_row[c] = ufpc.Merge(img_labels_row_prev_prev[c], img_labels_row_prev_prev[c + 2]);
                               // Action 11: Merge labels of block Q and S
#define ACTION_11 img_labels_row[c] = ufpc.Merge(img_labels_row_prev_prev[c], img_labels_row[c - 2]);
                               // Action 12: Merge labels of block R and S
#define ACTION_12 img_labels_row[c] = ufpc.Merge(img_labels_row_prev_prev[c + 2], img_labels_row[c - 2]);
                               // Action 13: Merge labels of block P, Q and R
#define ACTION_13 img_labels_row[c] = ufpc.Merge(ufpc.Merge(img_labels_row_prev_prev[c - 2], img_labels_row_prev_prev[c]), img_labels_row_prev_prev[c + 2]);
                               // Action 14: Merge labels of block P, Q and S
#define ACTION_14 img_labels_row[c] = ufpc.Merge(ufpc.Merge(img_labels_row_prev_prev[c - 2], img_labels_row_prev_prev[c]), img_labels_row[c - 2]);
                               //Action 15: Merge labels of block P, R and S
#define ACTION_15 img_labels_row[c] = ufpc.Merge(ufpc.Merge(img_labels_row_prev_prev[c - 2], img_labels_row_prev_prev[c + 2]), img_labels_row[c - 2]);
                               //Action 16: labels of block Q, R and S
#define ACTION_16 img_labels_row[c] = ufpc.Merge(ufpc.Merge(img_labels_row_prev_prev[c], img_labels_row_prev_prev[c + 2]), img_labels_row[c - 2]);
    }

    if (h == 1) {
        // Single line
        int r = 0;
        const unsigned char* const img_row = src.Ptr({ 0, 0, 0 });
        unsigned* const img_labels_row = reinterpret_cast<unsigned*>(tmp.Ptr({ 0, 0, 0 }));
        int c = -2;
#include "labeling_bolelli_2019_forest_singleline.inc"
    }
    else {
        // More than one line

        // First couple of lines
        {
            int r = 0;
            const unsigned char* const img_row = src.Ptr({ 0, 0, 0 });
            unsigned* const img_labels_row = reinterpret_cast<unsigned*>(tmp.Ptr({ 0, 0, 0 }));
            const unsigned char* const img_row_fol = img_row + src.strides_[1];

            int c = -2;

#include "labeling_bolelli_2019_forest_firstline.inc"
        }

        // Every other line but the last one if image has an odd number of rows
        for (int r = 2; r < e_rows; r += 2) {
            // Get rows pointer
            const unsigned char* const img_row = src.Ptr({ 0, r, 0 });
            const unsigned char* const img_row_prev = img_row - src.strides_[1];
            const unsigned char* const img_row_prev_prev = img_row_prev - src.strides_[1];
            const unsigned char* const img_row_fol = img_row + src.strides_[1];
            unsigned* const img_labels_row = reinterpret_cast<unsigned*>(tmp.Ptr({ 0, r, 0 }));
            unsigned* const img_labels_row_prev_prev = reinterpret_cast<unsigned*>((reinterpret_cast<uint8_t*>(img_labels_row) - 2 * tmp.strides_[1]));

            int c = -2;
            goto tree_0;

#include "labeling_bolelli_2019_forest.inc"
        }

        // Last line (in case the rows are odd)
        if (o_rows) {
            int r = h - 1;
            const unsigned char* const img_row = src.Ptr({ 0, r, 0 });
            const unsigned char* const img_row_prev = img_row - src.strides_[1];
            const unsigned char* const img_row_prev_prev = img_row_prev - src.strides_[1];
            unsigned* const img_labels_row = reinterpret_cast<unsigned*>(tmp.Ptr({ 0, r, 0 }));
            unsigned* const img_labels_row_prev_prev = reinterpret_cast<unsigned*>((reinterpret_cast<uint8_t*>(img_labels_row) - 2 * tmp.strides_[1]));

            int c = -2;
#include "labeling_bolelli_2019_forest_lastline.inc"
        }
    }

    // Undef Conditions and Actions
    {
#undef ACTION_1
#undef ACTION_2
#undef ACTION_3
#undef ACTION_4
#undef ACTION_5
#undef ACTION_6
#undef ACTION_7
#undef ACTION_8
#undef ACTION_9
#undef ACTION_10
#undef ACTION_11
#undef ACTION_12
#undef ACTION_13
#undef ACTION_14
#undef ACTION_15
#undef ACTION_16

#undef CONDITION_B
#undef CONDITION_C
#undef CONDITION_D
#undef CONDITION_E

#undef CONDITION_G
#undef CONDITION_H
#undef CONDITION_I
#undef CONDITION_J
#undef CONDITION_K

#undef CONDITION_M
#undef CONDITION_N
#undef CONDITION_O
#undef CONDITION_P

#undef CONDITION_R
#undef CONDITION_S
#undef CONDITION_T
    }

    // Flatten
    unsigned k = 1;
    for (unsigned i = 1; i < P_length; ++i) {
        if (P[i] < i) {
            P[i] = P[P[i]];
        }
        else {
            P[i] = k;
            k = k + 1;
        }
    }

    unsigned int n_labels_ = k;

    // Second scan
    int r = 0;
    for (; r < e_rows; r += 2) {
        // Get rows pointer
        const unsigned char* const img_row = src.Ptr({ 0, r, 0 });
        const unsigned char* const img_row_fol = img_row + src.strides_[1];

        unsigned* const img_labels_row = reinterpret_cast<unsigned*>(tmp.Ptr({ 0, r, 0 }));
        unsigned* const img_labels_row_fol = reinterpret_cast<unsigned*>((reinterpret_cast<uint8_t*>(img_labels_row) + tmp.strides_[1]));

        int c = 0;
        for (; c < e_cols; c += 2) {
            int iLabel = img_labels_row[c];
            if (iLabel > 0) {
                iLabel = P[iLabel];
                if (img_row[c] > 0)
                    img_labels_row[c] = iLabel;
                else
                    img_labels_row[c] = 0;
                if (img_row[c + 1] > 0)
                    img_labels_row[c + 1] = iLabel;
                else
                    img_labels_row[c + 1] = 0;
                if (img_row_fol[c] > 0)
                    img_labels_row_fol[c] = iLabel;
                else
                    img_labels_row_fol[c] = 0;
                if (img_row_fol[c + 1] > 0)
                    img_labels_row_fol[c + 1] = iLabel;
                else
                    img_labels_row_fol[c + 1] = 0;
            }
            else {
                img_labels_row[c] = 0;
                img_labels_row[c + 1] = 0;
                img_labels_row_fol[c] = 0;
                img_labels_row_fol[c + 1] = 0;
            }
        }
        // Last column if the number of columns is odd
        if (o_cols) {
            int iLabel = img_labels_row[c];
            if (iLabel > 0) {
                iLabel = P[iLabel];
                if (img_row[c] > 0)
                    img_labels_row[c] = iLabel;
                else
                    img_labels_row[c] = 0;
                if (img_row_fol[c] > 0)
                    img_labels_row_fol[c] = iLabel;
                else
                    img_labels_row_fol[c] = 0;
            }
            else {
                img_labels_row[c] = 0;
                img_labels_row_fol[c] = 0;
            }
        }
    }
    // Last row if the number of rows is odd
    if (o_rows) {
        // Get rows pointer
        const unsigned char* const img_row = src.Ptr({ 0, r, 0 });
        unsigned* const img_labels_row = reinterpret_cast<unsigned*>(tmp.Ptr({ 0, r, 0 }));

        int c = 0;
        for (; c < e_cols; c += 2) {
            int iLabel = img_labels_row[c];
            if (iLabel > 0) {
                iLabel = P[iLabel];
                if (img_row[c] > 0)
                    img_labels_row[c] = iLabel;
                else
                    img_labels_row[c] = 0;
                if (img_row[c + 1] > 0)
                    img_labels_row[c + 1] = iLabel;
                else
                    img_labels_row[c + 1] = 0;
            }
            else {
                img_labels_row[c] = 0;
                img_labels_row[c + 1] = 0;
            }
        }
        // Last column if the number of columns is odd
        if (o_cols) {
            int iLabel = img_labels_row[c];
            if (iLabel > 0) {
                iLabel = P[iLabel];
                if (img_row[c] > 0)
                    img_labels_row[c] = iLabel;
                else
                    img_labels_row[c] = 0;
            }
            else {
                img_labels_row[c] = 0;
            }
        }
    }

    delete[] P;
    dst = std::move(tmp);
}

void CpuHal::FindContours(const Image& src, vector<vector<ecvl::Point2i>>& contours)
{
    OpenCVAlwaysCheck(src);

    cv::Mat cv_src = ecvl::ImageToMat(src);

    vector<vector<cv::Point>> cv_contours;
    vector<cv::Vec4i> hierarchy;
    cv::findContours(cv_src, cv_contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    contours.resize(cv_contours.size());
    for (int i = 0; i < vsize(cv_contours); ++i) {
        vector<ecvl::Point2i> t(cv_contours[i].size());
        for (int j = 0; j < vsize(cv_contours[i]); ++j) {
            t[j] = Point2i{ cv_contours[i][j].x, cv_contours[i][j].y };
        }
        contours[i] = t;
    }
}

void CpuHal::Stack(const vector<Image>& src, Image& dst)
{
    const int n_images = vsize(src);
    const auto& src_0 = src[0];

    // If src is a vector of xyc Image
    if (src_0.channels_ == "xyc") {
        Image tmp({ src_0.dims_[0], src_0.dims_[1], n_images * src_0.dims_[2] }, src_0.elemtype_, "xyo", src_0.colortype_, src_0.spacings_, src_0.dev_);

        for (int i = 0; i < n_images; ++i) {
            for (int j = 0; j < src[i].Channels(); ++j) {
                memcpy(tmp.data_ + src[i].strides_[2] * (i + j * n_images), src[i].data_ + src[i].strides_[2] * j, src[i].strides_[2]);
            }
        }
        dst = std::move(tmp);
    }
    else {
        ECVL_ERROR_NOT_IMPLEMENTED
    }
}

void CpuHal::HConcat(const vector<Image>& src, Image& dst)
{
    const int n_images = vsize(src);
    const auto& src_0 = src[0];
    size_t c_pos = src_0.channels_.find('c');
    size_t x_pos = src_0.channels_.find('x');
    size_t y_pos = src_0.channels_.find('y');
    const int src_height = src_0.Height();
    const int src_channels = src_0.Channels();

    // calculate the width of resulting image
    vector<int> cumul_strides;
    int new_width = 0, src_stride_y_tot = 0;
    for (int i = 0; i < n_images; ++i) {
        cumul_strides.push_back(src_stride_y_tot);
        new_width += src[i].dims_[x_pos];
        src_stride_y_tot += src[i].strides_[y_pos];
    }

    vector<int> new_dims(src_0.dims_);
    new_dims[x_pos] = new_width;
    Image tmp(new_dims, src_0.elemtype_, src_0.channels_, src_0.colortype_, src_0.spacings_, src_0.dev_);

    // If src is a vector of xyc Image(s)
    if (src_0.channels_ == "xyc") {
        // 4x time faster than generic version below
        // Fill each tmp color plane by row
        for (int i = 0; i < src_0.Channels(); ++i) {
            for (int r = 0; r < src_0.dims_[1]; ++r) {
                for (int c = 0; c < n_images; ++c) {
                    memcpy(tmp.data_ + cumul_strides[c] + r * tmp.strides_[1] + i * tmp.strides_[2], src[c].data_ + r * src[c].strides_[1] + i * src[c].strides_[2], src[c].strides_[1]);
                }
            }
        }
    }
    else {
        int src_stride_x = src_0.strides_[x_pos];
        int src_stride_y = src_0.strides_[y_pos];
        vector<vector<uint8_t*>> src_vch(src_channels);
        vector<uint8_t*>tmp_vch(src_channels);

        // Get the pointers to channels starting pixels
        for (int c = 0; c < src_channels; ++c) {
            for (int i = 0; i < n_images; ++i) {
                src_vch[c].push_back(src[i].data_ + c * src[i].strides_[c_pos]);
            }
            tmp_vch[c] = tmp.data_ + c * tmp.strides_[c_pos];
        }

        for (int r = 0; r < src_height; ++r) {
            // Get the address of next row
            int tmp_pos = r * tmp.strides_[y_pos];
            int counter = 0;
            for (int i = 0; i < n_images; ++i) {
                int src_pos = r * src[i].strides_[y_pos];
                for (int c_i = 0; c_i < src[i].Width(); ++c_i) {
                    int p_tmp = tmp_pos + src_stride_x * counter++;
                    int p_src = src_pos + src_stride_x * c_i;
#define ECVL_TUPLE(type, ...) \
                case DataType::type: \
                    for (int ch = 0; ch < src_channels; ++ch) { \
                        *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p_tmp) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch][i] + p_src); \
                    } \
                    break;

                    switch (src_0.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
                    }

#undef ECVL_TUPLE
                }
            }
        }
    }
    dst = std::move(tmp);
}

void CpuHal::VConcat(const vector<Image>& src, Image& dst)
{
    const int n_images = vsize(src);
    const auto& src_0 = src[0];
    size_t c_pos = src_0.channels_.find('c');
    size_t x_pos = src_0.channels_.find('x');
    size_t y_pos = src_0.channels_.find('y');
    const int src_width = src_0.Width();
    const int src_channels = src_0.Channels();

    // calculate the width of resulting image
    vector<int> cumul_strides;
    int new_height = 0, src_stride_c_tot = 0;
    for (int i = 0; i < n_images; ++i) {
        cumul_strides.push_back(src_stride_c_tot);
        new_height += src[i].dims_[y_pos];
        src_stride_c_tot += src[i].strides_[c_pos];
    }

    vector<int> new_dims(src_0.dims_);
    new_dims[y_pos] = new_height;
    Image tmp(new_dims, src_0.elemtype_, src_0.channels_, src_0.colortype_, src_0.spacings_, src_0.dev_);

    // If src is a vector of xyc Image(s)
    if (src_0.channels_ == "xyc") {
        // 4x time faster than generic version below
        // Fill each tmp color plane concatenating every src color plane
        for (int i = 0; i < src_0.Channels(); ++i) {
            for (int c = 0; c < n_images; ++c) {
                memcpy(tmp.data_ + cumul_strides[c] + i * tmp.strides_[2], src[c].data_ + i * src[c].strides_[2], src[c].strides_[2]);
            }
        }
    }
    else {
        int src_stride_x = src_0.strides_[x_pos];
        vector<vector<uint8_t*>> src_vch(src_channels);
        vector<uint8_t*>tmp_vch(src_channels);

        // Get the pointers to channels starting pixels
        for (int c = 0; c < src_channels; ++c) {
            for (int i = 0; i < n_images; ++i) {
                src_vch[c].push_back(src[i].data_ + c * src[i].strides_[c_pos]);
            }
            tmp_vch[c] = tmp.data_ + c * tmp.strides_[c_pos];
        }

        for (int i = 0, offset = 0; i < n_images; offset += src[i].Height(), ++i) {
            for (int r = 0; r < src[i].Height(); ++r) {
                // Get the address of next row
                int tmp_pos = (offset + r) * src[i].strides_[y_pos];
                int src_pos = r * src[i].strides_[y_pos];
                for (int c = 0; c < src_width; ++c) {
                    int p_tmp = tmp_pos + src_stride_x * c;
                    int p_src = src_pos + src_stride_x * c;
#define ECVL_TUPLE(type, ...) \
                case DataType::type: \
                    for (int ch = 0; ch < src_channels; ++ch) { \
                        *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p_tmp) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch][i] + p_src); \
                    } \
                    break;

                    switch (src_0.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
                    }

#undef ECVL_TUPLE
                }
            }
        }
    }
    dst = std::move(tmp);
}

void CpuHal::Morphology(const Image& src, Image& dst, MorphType op, Image& kernel, Point2i anchor, int iterations, BorderType borderType, const int& borderValue)
{
    OpenCVAlwaysCheck(src);

    using namespace cv;
    Mat src_(ImageToMat(src));
    Mat kernel_(ImageToMat(kernel));
    Mat dst_;
    Point anchor_{ anchor[0], anchor[1] };

    int op_ = static_cast<int>(op);
    morphologyEx(src_, dst_, op_, kernel_, anchor_, iterations, static_cast<int>(borderType), borderValue);

    dst = MatToImage(dst_);
}

void CpuHal::Inpaint(const Image& src, Image& dst, const Image& inpaintMask, double inpaintRadius, InpaintType flag)
{
    OpenCVAlwaysCheck(src);

    using namespace cv;
    Mat src_(ImageToMat(src));
    Mat inpaintMask_(ImageToMat(inpaintMask));
    Mat dst_;
    int flag_ = static_cast<int>(flag);

    cv::inpaint(src_, inpaintMask_, dst_, inpaintRadius, flag_);

    dst = MatToImage(dst_);
}

void CpuHal::MeanStdDev(const Image& src, std::vector<double>& mean, std::vector<double>& stddev)
{
    OpenCVAlwaysCheck(src);

    using namespace cv;
    Mat src_(ImageToMat(src));
    Scalar mean_, stddev_;
    mean.clear();
    stddev.clear();

    meanStdDev(src_, mean_, stddev_);
    for (int i = 0; i < src.Channels(); ++i) {
        mean.push_back(mean_[i]);
        stddev.push_back(stddev_[i]);
    }
}

void CpuHal::Transpose(const Image& src, Image& dst)
{
    size_t c_pos = src.channels_.find('c');
    size_t x_pos = src.channels_.find('x');
    size_t y_pos = src.channels_.find('y');

    if (c_pos == string::npos || x_pos == string::npos || y_pos == string::npos) {
        ECVL_ERROR_WRONG_PARAMS("Malformed src image")
    }

    int src_width = src.Width();
    int src_height = src.Height();
    int src_channels = src.Channels();

    vector<int> new_dims(src.dims_);
    new_dims[x_pos] = src_height;
    new_dims[y_pos] = src_width;

    Image tmp(new_dims, src.elemtype_, src.channels_, src.colortype_, src.spacings_, src.dev_);

    int src_stride_c = src.strides_[c_pos];
    int tmp_stride_c = tmp.strides_[c_pos];
    int src_stride_x = src.strides_[x_pos];
    int tmp_stride_x = tmp.strides_[x_pos];
    int src_stride_y = src.strides_[y_pos];
    int tmp_stride_y = tmp.strides_[y_pos];
    vector<uint8_t*> src_vch(src_channels), tmp_vch(src_channels);

    // Get the pointers to channels starting pixels
    for (int i = 0; i < src_channels; ++i) {
        src_vch[i] = src.data_ + i * src_stride_c;
        tmp_vch[i] = tmp.data_ + i * tmp_stride_c;
    }

    for (int c = 0; c < src_width; ++c) {
        // Get the address of pixels in current row
        int pos = src_stride_x * c;
        int pos_dst = tmp_stride_y * c;
        for (int r = 0; r < src_height; ++r) {
            // Get the address of next row
            int p1 = pos + r * src_stride_y;
            int p2 = pos_dst + r * tmp_stride_x;

#define ECVL_TUPLE(type, ...) \
            case DataType::type: \
                for (int ch = 0; ch < src_channels; ++ch) { \
                    *reinterpret_cast<TypeInfo_t<DataType::type>*>(tmp_vch[ch] + p2) = *reinterpret_cast<TypeInfo_t<DataType::type>*>(src_vch[ch] + p1); \
                } \
                break;

            switch (src.elemtype_) {
#include "ecvl/core/datatype_existing_tuples.inc.h"
            }

#undef ECVL_TUPLE
        }
    }

    dst = std::move(tmp);
}

void FillCoordsVector(vector<float>& v, vector<float>& steps, int size, int num_steps)
{
    int step = size / num_steps;
    int prev = 0, start = 0, end = 0, x = 0;
    float cur = 0, diff = 0, div = 0;

    for (int idx = 0; idx < num_steps; ++idx) {
        x = idx * step;
        start = x;
        end = x + step;
        if (end > size) {
            end = size;
            cur = static_cast<float>(size);
        }
        else {
            cur = prev + step * steps[idx];
        }

        diff = cur - prev;
        div = diff / (step - 1);
        v[start] = static_cast<float>(prev);

        for (int j = start; j < end - 1; ++j) {
            v[j + 1] = v[j] + div;
        }
        prev = static_cast<int>(cur);
    }
}

void CpuHal::GridDistortion(const Image& src, Image& dst, int num_steps, const std::array<float, 2>& distort_limit, InterpolationType interp, BorderType borderType, const int& borderValue)
{
    OpenCVAlwaysCheck(src);

    std::default_random_engine re(std::random_device{}());
    vector<float> xsteps, ysteps;
    for (int i = 0; i < num_steps + 1; ++i) {
        xsteps.push_back(1 + std::uniform_real_distribution<float>(distort_limit[0], distort_limit[1])(re));
        ysteps.push_back(1 + std::uniform_real_distribution<float>(distort_limit[0], distort_limit[1])(re));
    }

    int height = src.Height();
    int width = src.Width();

    int x_step = width / num_steps;
    vector<float> xx(width, 0.);
    int y_step = height / num_steps;
    vector<float> yy(height, 0.);

    FillCoordsVector(xx, xsteps, width, num_steps);
    FillCoordsVector(yy, ysteps, height, num_steps);

    Image map_x({ width, height, 1 }, DataType::float32, "xyc", ColorType::none);
    Image map_y({ width, height, 1 }, DataType::float32, "xyc", ColorType::none);

    for (int i = 0; i < height; ++i) {
        memcpy(map_x.data_ + (width * i * map_x.elemsize_), xx.data(), width * map_x.elemsize_);
    }
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            memcpy(map_y.data_ + map_y.elemsize_ * c + map_y.strides_[1] * r, yy.data() + r, map_y.elemsize_);
        }
    }
    
    if (src.elemtype_ == DataType::int8 || src.elemtype_ == DataType::int32) {
        interp = InterpolationType::nearest;
    }

    cv::Mat tmp;
    cv::remap(ImageToMat(src), tmp, ImageToMat(map_x), ImageToMat(map_y), GetOpenCVInterpolation(interp), static_cast<int>(borderType), borderValue);
    dst = MatToImage(tmp);
    if (dst.colortype_ != src.colortype_) {
        ChangeColorSpace(dst, dst, src.colortype_);
    }
}

void CpuHal::ElasticTransform(const Image& src, Image& dst, float alpha, float sigma, InterpolationType interp, BorderType borderType, const int& borderValue)
{
    OpenCVAlwaysCheck(src);

    std::default_random_engine re(std::random_device{}());

    int height = src.Height();
    int width = src.Width();

    Image dx({ src.Width(), src.Height(), 1 }, DataType::float32, "xyc", ColorType::none);
    auto it = dx.Begin<float>(), e = dx.End<float>();
    for (; it != e; ++it) {
        *it = std::uniform_real_distribution<float>(-1, 1)(re);
    }
    ecvl::GaussianBlur(dx, dx, 17, 17, sigma);
    ecvl::Mul(dx, alpha, dx, dx.elemtype_);

    Image dy({ src.Width(), src.Height(), 1 }, DataType::float32, "xyc", ColorType::none);
    it = dy.Begin<float>(), e = dy.End<float>();
    for (; it != e; ++it) {
        *it = std::uniform_real_distribution<float>(-1, 1)(re);
    }
    ecvl::GaussianBlur(dy, dy, 17, 17, sigma);
    ecvl::Mul(dy, alpha, dy, dy.elemtype_);

    vector<float> width_range(width);
    vector<float> height_range(height);
    iota(width_range.begin(), width_range.end(), 0.f);
    iota(height_range.begin(), height_range.end(), 0.f);

    Image map_x({ width, height, 1 }, DataType::float32, "xyc", ColorType::none);
    Image map_y({ width, height, 1 }, DataType::float32, "xyc", ColorType::none);

    for (int i = 0; i < height; ++i) {
        memcpy(map_x.data_ + (width * i * map_x.elemsize_), width_range.data(), width * map_x.elemsize_);
    }
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            memcpy(map_y.data_ + map_y.elemsize_ * c + map_y.strides_[1] * r, height_range.data() + r, map_y.elemsize_);
        }
    }

    ecvl::Add(map_x, dx, map_x, map_x.elemtype_);
    ecvl::Add(map_y, dy, map_y, map_y.elemtype_);

    if (src.elemtype_ == DataType::int8 || src.elemtype_ == DataType::int32) {
        interp = InterpolationType::nearest;
    }

    cv::Mat tmp;
    cv::remap(ImageToMat(src), tmp, ImageToMat(map_x), ImageToMat(map_y), GetOpenCVInterpolation(interp), static_cast<int>(borderType), borderValue);
    dst = MatToImage(tmp);
    if (dst.colortype_ != src.colortype_) {
        ChangeColorSpace(dst, dst, src.colortype_);
    }
}
} // namespace ecvl