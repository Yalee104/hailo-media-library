#pragma once

#include "stages.hpp"
#include "utils.hpp"

#define CROP_ROUND_DOWN_TO_EVEN(n) ((n) % 2 == 0 ? (n) : (n) - 1)

struct crop_resize_dims_extended_t : dsp_utils::crop_resize_dims_t {    
    bool                        perform_letterbox = false;
    dsp_letterbox_alignment_t   alignment = DSP_LETTERBOX_MIDDLE; //DSP_LETTERBOX_MIDDLE or DSP_LETTERBOX_UP_LEFT,
    dsp_color_t                 fillcolor = {.y = 0, .u = 128, .v = 128}; //uint8_t y, u, v
};

class DspBaseCropStage : public ProducableBufferStage
{
private:
    MediaLibraryBufferPoolPtr m_buffer_pool;
    int m_output_pool_size;

protected:
    int     m_max_output_width;
    int     m_max_output_height;

public:
    DspBaseCropStage(std::string name, int output_pool_size, int max_output_width, int max_output_height, size_t queue_size, bool leaky=true,
     int non_leaky_timeout_in_ms = 1000) : ProducableBufferStage(name, queue_size, leaky, non_leaky_timeout_in_ms), 
             m_output_pool_size(output_pool_size), m_max_output_width(max_output_width), m_max_output_height(max_output_height) {}

    int init() override
    {
        auto bytes_per_line = dsp_utils::get_dsp_desired_stride_from_width(m_max_output_width);
        m_buffer_pool = std::make_shared<MediaLibraryBufferPool>(m_max_output_width, m_max_output_height, DSP_IMAGE_FORMAT_NV12,
                                                                 m_output_pool_size, CMA, bytes_per_line);

        if (m_buffer_pool->init() != MEDIA_LIBRARY_SUCCESS)
        {
            return ERROR;
        }

        return SUCCESS;
    }

    virtual void prepare_crops(BufferPtr input_buffer, std::vector<crop_resize_dims_extended_t> &crop_resize_dims) = 0;

    virtual void post_crop(BufferPtr input_buffer) {}

    virtual dsp_image_properties_t get_dsp_image_properties(BufferPtr input_buffer)
    {
        return *input_buffer->media_lib_buffers_list[MediaLibraryBufferType::Stream4K]->hailo_pix_buffer.get();
    }

    int process(BufferPtr data) override
    {

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        std::vector<crop_resize_dims_extended_t> crop_resize_dims;
        prepare_crops(data, crop_resize_dims);

        for (auto &dims : crop_resize_dims)
        {
            std::chrono::steady_clock::time_point begin_crop = std::chrono::steady_clock::now();

            hailo_media_library_buffer* cropped_buffer = new hailo_media_library_buffer;
            if (m_buffer_pool->acquire_buffer(*cropped_buffer) != MEDIA_LIBRARY_SUCCESS)
            {
                std::cerr << "Failed to acquire buffer" << std::endl;
                return ERROR;
            }

            dsp_image_properties_t input_image_properties = get_dsp_image_properties(data);

            dsp_utils::crop_resize_dims_t targetarg = dims;

            stletterbox letterbox;
            letterbox.is_performed = false;
            if (dims.perform_letterbox) {

                letterbox.is_performed = true;

                //To use the dsp letterbox we will need to provide crop (at least for the release version 1.3.0)
                //Therefore we simply set the crop to crop the entire source image if crop is not required from application level
                if (dims.perform_crop == 0) {

                    targetarg.perform_crop = 1;
                    targetarg.crop_start_x = 0;
                    targetarg.crop_start_y = 0;
                    targetarg.crop_end_x = CROP_ROUND_DOWN_TO_EVEN(input_image_properties.width);
                    targetarg.crop_end_y = CROP_ROUND_DOWN_TO_EVEN(input_image_properties.height);
                }

                dsp_letterbox_properties_t dspletterbox = {.alignment =  dims.alignment, .color = dims.fillcolor};

                dsp_utils::perform_crop_and_resize_letterbox(   &input_image_properties, cropped_buffer->hailo_pix_buffer.get(), 
                                                                targetarg, INTERPOLATION_TYPE_BILINEAR, dspletterbox);

                float original_width = (float)(targetarg.crop_end_x - targetarg.crop_start_x);
                float original_height = (float)(targetarg.crop_end_y - targetarg.crop_start_y);

                float scale_width = (float)dims.destination_width / original_width;
                float scale_height = (float)dims.destination_height / original_height;
                float scale_min = (scale_width >= scale_height) ? scale_height : scale_width;

                letterbox.width = scale_min * original_width;
                letterbox.height = scale_min * original_height;

                if (dims.alignment == DSP_LETTERBOX_MIDDLE) {
                    letterbox.offset_x = (dims.destination_width - letterbox.width) / 2;
                    letterbox.offset_y = (dims.destination_height - letterbox.height) / 2;
                }
                else {
                    //Assume DSP_LETTERBOX_UP_LEFT
                    letterbox.offset_x = 0;
                    letterbox.offset_y = 0;
                }
            }
            else {
                dsp_utils::perform_crop_and_resize( &input_image_properties, cropped_buffer->hailo_pix_buffer.get(), 
                                                    targetarg, INTERPOLATION_TYPE_BILINEAR);
            }
            

            BufferPtr cropped_buffer_shared = std::make_shared<Buffer>(false);
            cropped_buffer_shared->add_media_lib_buffer(MediaLibraryBufferType::Cropped, HailoMediaLibraryBufferPtr(cropped_buffer));
            cropped_buffer_shared->copy_media_lib_buffers(data);

            std::shared_ptr<CroppedBufferMetadata> metadata = std::make_shared<CroppedBufferMetadata>(  dims.destination_width,
                                                                                                        dims.destination_height, 
                                                                                                        dims.crop_start_x,
                                                                                                        dims.crop_end_x, 
                                                                                                        dims.crop_start_y, 
                                                                                                        dims.crop_end_y,
                                                                                                        letterbox);

            cropped_buffer_shared->append_metadata(metadata);

            send_to_subscribers(cropped_buffer_shared);

            std::chrono::steady_clock::time_point end_crop = std::chrono::steady_clock::now();

            if (PRINT_STATS)
            {
                std::cout << "----> Crop and resize time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end_crop - begin_crop).count() << "[milliseconds]" << std::endl;
            }
        }

        post_crop(data);

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

        if (PRINT_STATS)
        {
            std::cout << "Crop and resize time = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[milliseconds]" << std::endl;
        }

        return SUCCESS;
    }
};

class BBoxCropStage : public DspBaseCropStage
{
public:
    BBoxCropStage(std::string name, size_t queue_size, int output_pool_size, int max_output_width, int max_output_height) : 
        DspBaseCropStage(name, output_pool_size, max_output_width, max_output_height, queue_size) {}

    void prepare_crops(BufferPtr input_buffer, std::vector<crop_resize_dims_extended_t> &crop_resize_dims) override
    {        
        auto bbox_metadata = get_metadata<BBoxBufferMetadata>(input_buffer, BufferMetadataType::BBox);
        if (!bbox_metadata) {
            std::cerr << "Failed to get bbox metadata" << std::endl;
            return;
        }
        
        std::vector<HailoDetectionPtr> bboxes = bbox_metadata->m_bboxes;

        for (auto &HDbbox : bboxes)
        {
            HailoBBox bbox = HDbbox->get_bbox();

            if (static_cast<int>(bbox.width()) > m_max_output_width || static_cast<int>(bbox.height()) > m_max_output_height)
            {
                std::cerr << "BBox is too big, skipping" << std::endl;
                continue;
            }

            crop_resize_dims_extended_t crop_resize_dim;
            crop_resize_dim.perform_crop = 1;
            crop_resize_dim.crop_start_x = bbox.xmin();
            crop_resize_dim.crop_end_x = bbox.xmin() + bbox.width();
            crop_resize_dim.crop_start_y = bbox.ymin();
            crop_resize_dim.crop_end_y = bbox.ymin() + bbox.height();
            crop_resize_dim.destination_width = bbox.width();
            crop_resize_dim.destination_height = bbox.height();

            crop_resize_dims.push_back(crop_resize_dim);
        }
    }
};