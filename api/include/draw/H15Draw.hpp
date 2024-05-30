/**
 * Copyright (c) 2020-2023 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/

/* 
 * File:   H15Draw.hpp
 * Author: Aaron Lee (this file uses the original hailomat.hpp from the Hailo)
 * 
 * Created on Oct 27, 2023, 11:32 AM
 */

#pragma once

#include "media_library/buffer_pool.hpp"
#include <hailo/hailodsp.h>
#include <opencv2/opencv.hpp>

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define CLIP(x) (CLAMP(x, 0, 255))

// Transformations were taken from https://stackoverflow.com/questions/17892346/how-to-convert-rgb-yuv-rgb-both-ways.
#define RGB2Y(R, G, B) CLIP((0.257 * (R) + 0.504 * (G) + 0.098 * (B)) + 16)
#define RGB2U(R, G, B) CLIP((-0.148 * (R)-0.291 * (G) + 0.439 * (B)) + 128)
#define RGB2V(R, G, B) CLIP((0.439 * (R)-0.368 * (G)-0.071 * (B)) + 128)

typedef enum
{
    HAILO_MAT_NONE = -1,
    HAILO_MAT_RGB,
    HAILO_MAT_RGBA,
    HAILO_MAT_YUY2,
    HAILO_MAT_NV12
} hailo_mat_t;

typedef enum
{
    HAILO_DRAW_FAILED = -2,
    HAILO_DRAW_INCOMPATIBLE_BUFFER_FORMAT = -1,
    HAILO_DRAW_SUCCESS = 0,
} HailoDrawRetCode;

typedef enum
{
    NONE = -1,
    VERTICAL,
    HORIZONTAL,
    DIAGONAL,
    ANTI_DIAGONAL,
} LineOrientation;

inline LineOrientation line_orientation(cv::Point point1, cv::Point point2)
{
    if (point1.x == point2.x)
        return VERTICAL;
    else if (point1.y == point2.y)
        return HORIZONTAL;
    else if (point1.x < point2.x && point1.y < point2.y)
        return DIAGONAL;
    else if (point1.x < point2.x && point1.y > point2.y)
        return ANTI_DIAGONAL;
    else
        return NONE;
}

inline int floor_to_even_number(int x)
{
    /*
    The expression x &~1 in C++ performs a bitwise AND operation between the number x and the number ~1(bitwise negation of 1).
    In binary representation, the number 1 is represented as 0000 0001, and its negation, ~1, is equal to 1111 1110.
    The bitwise AND operation between x and ~1 zeros out the least significant bit of x,
    effectively rounding it down to the nearest even number.
    This is because any odd number in binary representation will have its least significant bit set to 1,
    and ANDing it with ~1 will zero out that bit.
    */
    return x & ~1;
}

class HailoMat
{
protected:
    uint m_height;
    uint m_width;
    uint m_native_height;
    uint m_native_width;
    uint m_stride;
    int m_line_thickness;
    int m_font_thickness;
    std::vector<cv::Mat> m_matrices;

public:
    HailoMat(uint height, uint width, uint stride, int line_thickness = 1, int font_thickness = 1) : m_height(height),
                                                                                                     m_width(width),
                                                                                                     m_native_height(height),
                                                                                                     m_native_width(width),
                                                                                                     m_stride(stride),
                                                                                                     m_line_thickness(line_thickness),
                                                                                                     m_font_thickness(font_thickness){};
    HailoMat() : m_height(0), m_width(0), m_native_height(0), m_native_width(0), m_stride(0), m_line_thickness(0), m_font_thickness(0){};
    virtual ~HailoMat() = default;
    uint width() { return m_width; };
    uint height() { return m_height; };
    uint native_width() { return m_native_width; };
    uint native_height() { return m_native_height; };
    std::vector<cv::Mat> &get_matrices() { return m_matrices; }
    virtual HailoDrawRetCode draw_rectangle(cv::Rect rect, const cv::Scalar color) = 0;
    virtual HailoDrawRetCode draw_text(std::string text, cv::Point position, double font_scale, int font_thickness, const cv::Scalar color) = 0;
    virtual HailoDrawRetCode draw_line(cv::Point point1, cv::Point point2, const cv::Scalar color, int thickness, int line_type) = 0;
    virtual HailoDrawRetCode draw_ellipse(cv::Point center, cv::Size axes, double angle, double start_angle, double end_angle, const cv::Scalar color, int thickness) = 0;

    /**
     * @brief Get the type of mat
     *
     * @return hailo_mat_t - The type of the mat.
     */
    virtual hailo_mat_t get_type() = 0;
};


/* TBD To be ported to support HailoMediaLibraryBufferPtr if needed in the future

class H15RGBDraw : public HailoMat
{
protected:
    std::string m_name;

public:
    H15RGBDraw(uint8_t *buffer, uint height, uint width, uint stride, int line_thickness = 1, int font_thickness = 1, std::string name = "H15RGBDraw") : HailoMat(height, width, stride, line_thickness, font_thickness)
    {
        m_name = name;
        cv::Mat mat = cv::Mat(m_height, m_width, CV_8UC3, buffer, m_stride);
        m_matrices.push_back(mat);
    };
    H15RGBDraw(cv::Mat mat, std::string name, int line_thickness = 1, int font_thickness = 1)
    {
        m_matrices.push_back(mat);
        m_name = name;
        m_height = mat.rows;
        m_width = mat.cols;
        m_stride = mat.step;
        m_native_height = m_height;
        m_native_width = m_width;
        m_line_thickness = line_thickness;
        m_font_thickness = font_thickness;
    }
    virtual hailo_mat_t get_type()
    {
        return HAILO_MAT_RGB;
    }
    virtual std::string get_name() const
    {
        return m_name;
    }
    virtual void draw_rectangle(cv::Rect rect, const cv::Scalar color)
    {
        cv::rectangle(m_matrices[0], rect, color, m_line_thickness);
    }
    virtual void draw_text(std::string text, cv::Point position, double font_scale, int font_thickness, const cv::Scalar color)
    {
        cv::putText(m_matrices[0], text, position, cv::FONT_HERSHEY_SIMPLEX, font_scale, color, m_font_thickness);
    }
    virtual void draw_line(cv::Point point1, cv::Point point2, const cv::Scalar color, int thickness, int line_type)
    {
        cv::line(m_matrices[0], point1, point2, color, thickness, line_type);
    }
    virtual void draw_ellipse(cv::Point center, cv::Size axes, double angle, double start_angle, double end_angle, const cv::Scalar color, int thickness)
    {
        cv::ellipse(m_matrices[0], center, axes, angle, start_angle, end_angle, color, thickness);
    }

    virtual ~H15RGBDraw()
    {
        for (auto &mat : m_matrices)
        {
            mat.release();
        }
        m_matrices.clear();
    }
};

*/


class H15NV12Draw : public HailoMat
{
    /**
        NV12 Layout in memory (planar YUV 4:2:0):

        +-----+-----+-----+-----+-----+-----+
        | Y0  | Y1  | Y2  | Y3  | Y4  | Y5  |
        +-----+-----+-----+-----+-----+-----+
        | Y6  | ... | ... | ... | ... | ... |
        +-----+-----+-----+-----+-----+-----+
        | Y12 | ... | ... | ... | ... | ... |
        +-----+-----+-----+-----+-----+-----+
        | Y18 | ... | ... | ... | ... | ... |
        +-----+-----+-----+-----+-----+-----+
        | Y24 | ... | ... | ... | ... | ... |
        +-----+-----+-----+-----+-----+-----+
        | Y30 | Y31 | Y32 | Y33 | Y34 | Y35 |
        +-----+-----+-----+-----+-----+-----+
        | U0  | V0  | U1  | V1  | U2  | V2  |
        +-----+-----+-----+-----+-----+-----+
        | U3  | V3  | ... | ... | ... | ... |
        +-----+-----+-----+-----+-----+-----+
        | U6  | V6  | U7  | V7  | U8  | V8  |
        +-----+-----+-----+-----+-----+-----+
    */
protected:
    
    HailoMediaLibraryBufferPtr m_buffer = nullptr;
    HailoDrawRetCode    m_StateCode = HAILO_DRAW_INCOMPATIBLE_BUFFER_FORMAT;
    uint m_y_stride;
    uint m_uv_stride;
    cv::Scalar get_nv12_color(cv::Scalar rgb_color)
    {
        uint r = rgb_color[0];
        uint g = rgb_color[1];
        uint b = rgb_color[2];
        uint y = RGB2Y(r, g, b);
        uint u = RGB2U(r, g, b);
        uint v = RGB2V(r, g, b);
        return cv::Scalar(y, u, v);
    }

public:
    H15NV12Draw(HailoMediaLibraryBufferPtr buffer, int line_thickness = 1, int font_thickness = 1) : 
                HailoMat(buffer->hailo_pix_buffer->height , buffer->hailo_pix_buffer->width, buffer->hailo_pix_buffer->width, line_thickness, font_thickness),
                m_buffer(buffer)
    {
        
        if (buffer->hailo_pix_buffer->format == DSP_IMAGE_FORMAT_NV12)
        {
            m_height = (m_height * 3 / 2);
            m_y_stride = buffer->get_plane_stride(0);
            m_uv_stride = buffer->get_plane_stride(1);
            void* plane0 = buffer->get_plane(0);
            void* plane1 = buffer->get_plane(1);

            /*
            std::cout << "Plane 0 Size: " << buffer->get_plane_size(0) << std::endl;
            std::cout << "Plane 1 Size: " << buffer->get_plane_size(1) << std::endl;
            std::cout << "Plane 0 Stride: " << buffer->get_plane_stride(0) << std::endl;
            std::cout << "Plane 1 Stride: " << buffer->get_plane_stride(1) << std::endl;
            std::cout << "Plane 0 address in hex: " << std::hex << plane0 << std::endl;
            std::cout << "Plane 1 address in hex: " << std::hex << plane1 << std::endl;
            */
        
            cv::Mat y_plane_mat = cv::Mat(m_native_height, m_width, CV_8UC1, plane0, m_y_stride);
            cv::Mat uv_plane_mat = cv::Mat(m_native_height / 2, m_native_width / 2, CV_8UC2, plane1, m_uv_stride);
            m_matrices.push_back(y_plane_mat);
            m_matrices.push_back(uv_plane_mat);

            m_StateCode = HAILO_DRAW_SUCCESS;
        }
    };

    bool draw_start()
    {
        if (!m_buffer)
            return false;
        
        m_buffer->sync_start();

        return true;
    }

    bool draw_end()
    {
        if (!m_buffer)
            return false;
        
        m_buffer->sync_end();

        return true;
    }

    virtual hailo_mat_t get_type()
    {
        return HAILO_MAT_NV12;
    }
    virtual HailoDrawRetCode draw_rectangle(cv::Rect rect, const cv::Scalar color)
    {
        if (m_StateCode == HAILO_DRAW_INCOMPATIBLE_BUFFER_FORMAT)
            return m_StateCode;

        cv::Scalar yuv_color = get_nv12_color(color);
        uint thickness = m_line_thickness > 1 ? m_line_thickness / 2 : 1;
        // always floor the rect coordinates to even numbers to avoid drawing on the wrong pixel
        int y_plane_rect_x = floor_to_even_number(rect.x);
        int y_plane_rect_y = floor_to_even_number(rect.y);
        int y_plane_rect_width = floor_to_even_number(rect.width);
        int y_plane_rect_height = floor_to_even_number(rect.height);

        cv::Rect y1_rect = cv::Rect(y_plane_rect_x, y_plane_rect_y, y_plane_rect_width, y_plane_rect_height);
        cv::Rect y2_rect = cv::Rect(y_plane_rect_x + 1, y_plane_rect_y + 1, y_plane_rect_width - 2, y_plane_rect_height - 2);
        cv::rectangle(m_matrices[0], y1_rect, cv::Scalar(yuv_color[0]), thickness);
        cv::rectangle(m_matrices[0], y2_rect, cv::Scalar(yuv_color[0]), thickness);

        cv::Rect uv_rect = cv::Rect(y_plane_rect_x / 2, y_plane_rect_y / 2, y_plane_rect_width / 2, y_plane_rect_height / 2);
        cv::rectangle(m_matrices[1], uv_rect, cv::Scalar(yuv_color[1], yuv_color[2]), thickness);

        return HAILO_DRAW_SUCCESS;
    }

    virtual HailoDrawRetCode draw_text(std::string text, cv::Point position, double font_scale, int font_thickness, const cv::Scalar color)
    {
        if (m_StateCode == HAILO_DRAW_INCOMPATIBLE_BUFFER_FORMAT)
            return m_StateCode;
        
        cv::Scalar yuv_color = get_nv12_color(color);
        cv::Point y_position = cv::Point(position.x, position.y);
        cv::Point uv_position = cv::Point(position.x / 2, position.y / 2);
        cv::putText(m_matrices[0], text, y_position, cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(yuv_color[0]), font_thickness);
        cv::putText(m_matrices[1], text, uv_position, cv::FONT_HERSHEY_SIMPLEX, font_scale / 2, cv::Scalar(yuv_color[1], yuv_color[2]), font_thickness / 2);

        return HAILO_DRAW_SUCCESS;
    };

    virtual HailoDrawRetCode draw_line(cv::Point point1, cv::Point point2, const cv::Scalar color, int thickness, int line_type)
    {
        if (m_StateCode == HAILO_DRAW_INCOMPATIBLE_BUFFER_FORMAT)
            return m_StateCode;

        cv::Scalar yuv_color = get_nv12_color(color);

        int y_plane_x1_value = floor_to_even_number(point1.x);
        int y_plane_y1_value = floor_to_even_number(point1.y);
        int y_plane_x2_value = floor_to_even_number(point2.x);
        int y_plane_y2_value = floor_to_even_number(point2.y);

        cv::line(m_matrices[1], cv::Point(y_plane_x1_value / 2, y_plane_y1_value / 2), cv::Point(y_plane_x2_value / 2, y_plane_y2_value / 2), cv::Scalar(yuv_color[1], yuv_color[2]), thickness, line_type);

        switch (line_orientation(point1, point2))
        {
        case HORIZONTAL:
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value, y_plane_y1_value), cv::Point(y_plane_x2_value, y_plane_y2_value), cv::Scalar(yuv_color[0]), thickness, line_type);
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value, y_plane_y1_value + 1), cv::Point(y_plane_x2_value, y_plane_y2_value + 1), cv::Scalar(yuv_color[0]), thickness, line_type);
            break;
        case VERTICAL:
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value, y_plane_y1_value), cv::Point(y_plane_x2_value, y_plane_y2_value), cv::Scalar(yuv_color[0]), thickness, line_type);
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value + 1, y_plane_y1_value), cv::Point(y_plane_x2_value + 1, y_plane_y2_value), cv::Scalar(yuv_color[0]), thickness, line_type);
            break;
        case DIAGONAL:
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value, y_plane_y1_value), cv::Point(y_plane_x2_value, y_plane_y2_value), cv::Scalar(yuv_color[0]), thickness, line_type);
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value + 1, y_plane_y1_value + 1), cv::Point(y_plane_x2_value + 1, y_plane_y2_value + 1), cv::Scalar(yuv_color[0]), thickness, line_type);
            break;
        case ANTI_DIAGONAL:
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value, y_plane_y1_value), cv::Point(y_plane_x2_value, y_plane_y2_value), cv::Scalar(yuv_color[0]), thickness, line_type);
            cv::line(m_matrices[0], cv::Point(y_plane_x1_value + 1, y_plane_y1_value), cv::Point(y_plane_x2_value + 1, y_plane_y2_value), cv::Scalar(yuv_color[0]), thickness, line_type);
            break;
        default:
            break;
        }

        return HAILO_DRAW_SUCCESS;
    }

    virtual HailoDrawRetCode draw_ellipse(cv::Point center, cv::Size axes, double angle, double start_angle, double end_angle, const cv::Scalar color, int thickness)
    {
        if (m_StateCode == HAILO_DRAW_INCOMPATIBLE_BUFFER_FORMAT)
            return m_StateCode;

        // Wrap the mat with Y and UV channel windows
        cv::Scalar yuv_color = get_nv12_color(color);

        cv::Point y_position = cv::Point(floor_to_even_number(center.x), floor_to_even_number(center.y));
        cv::Point uv_position = cv::Point(floor_to_even_number(center.x) / 2, floor_to_even_number(center.y) / 2);

        cv::ellipse(m_matrices[0], y_position, {floor_to_even_number(axes.width), floor_to_even_number(axes.height)}, angle, start_angle, end_angle, cv::Scalar(yuv_color[0]), thickness / 2);
        cv::ellipse(m_matrices[0], y_position, {floor_to_even_number(axes.width) + 1, floor_to_even_number(axes.height) + 1}, angle, start_angle, end_angle, cv::Scalar(yuv_color[0]), thickness / 2);
        cv::ellipse(m_matrices[1], uv_position, axes / 2, angle, start_angle, end_angle, cv::Scalar(yuv_color[1], yuv_color[2]), thickness);

        return HAILO_DRAW_SUCCESS;
    }

    virtual ~H15NV12Draw()
    {
        m_matrices.clear(); // this will call the destructor of each mat
    }
};