#pragma once

#include "base.hpp"

class QuantInfoBufferMetadata : public BufferMetadata
{
public:
    std::vector<hailo_quant_info_t> m_quant_info;

    QuantInfoBufferMetadata(std::vector<hailo_quant_info_t> quant_info) : BufferMetadata(BufferMetadataType::QuantizationInfo),
    m_quant_info(quant_info) { }
};
using QuantInfoBufferMetadataPtr = std::shared_ptr<QuantInfoBufferMetadata>;


class BBoxBufferMetadata : public BufferMetadata
{
public:
    size_t                          m_network_input_width;
    size_t                          m_network_input_height;
    std::vector<HailoDetectionPtr>  m_bboxes;
    stletterbox                     m_letterbox;

    BBoxBufferMetadata(size_t netInputWidth, size_t netInputHeight, std::vector<HailoDetectionPtr> bboxes, stletterbox &letterbox) : BufferMetadata(BufferMetadataType::BBox),
    m_network_input_width(netInputWidth), m_network_input_height(netInputHeight), m_bboxes(bboxes), m_letterbox(letterbox) { }
};
using BBoxBufferMetadataPtr = std::shared_ptr<BBoxBufferMetadata>;


class CroppedBufferMetadata : public BufferMetadata
{
public:

    size_t destination_width;
    size_t destination_height;
    size_t crop_start_x;
    size_t crop_end_x;
    size_t crop_start_y;
    size_t crop_end_y;
    stletterbox letterbox;

    CroppedBufferMetadata(size_t destination_width, size_t destination_height, size_t crop_start_x, size_t crop_end_x, size_t crop_start_y, size_t crop_end_y, stletterbox letterbox) :
        BufferMetadata(BufferMetadataType::Cropped), 
        destination_width(destination_width), destination_height(destination_height), 
        crop_start_x(crop_start_x), crop_end_x(crop_end_x), crop_start_y(crop_start_y), crop_end_y(crop_end_y), letterbox(letterbox)
        {
        }

    int get_width()
    {
        return crop_end_x - crop_start_x;
    }

    int get_height()
    {
        return crop_end_y - crop_start_y;
    }

};
using CroppedBufferMetadataPtr = std::shared_ptr<CroppedBufferMetadata>; 
