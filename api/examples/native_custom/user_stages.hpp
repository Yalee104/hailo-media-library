#pragma once

#include "buffer_utils.hpp"
#include "infra/stages.hpp"
#include "infra/dsp_stages.hpp"

#include "Utils/yolo-nms-decoder/yolo_nms_decoder.hpp"
#include "draw/H15Draw.hpp"
#include "ObjectDetectClassToString.hpp"


class DummyStage : public ProducableStage<BufferPtr, BufferPtr>
{
public:
    DummyStage(std::string name, size_t queue_size) : ProducableStage(name, queue_size, drop_buffer) { }

    int process(BufferPtr data) override
    {
        return SUCCESS;
    }
};


class DetectorResizeStage : public DspBaseCropStage
{
    int m_resized_width;
    int m_resized_height;

public: 
    DetectorResizeStage(std::string name, size_t queue_size, int output_pool_size, int resized_width, int resized_height ) : 
        DspBaseCropStage(name, output_pool_size, resized_width, resized_height, queue_size), m_resized_width(resized_width), m_resized_height(resized_height) { }

    dsp_image_properties_t get_dsp_image_properties(BufferPtr buffer)
    {
        // Getting the image_properties from 4K media lib buffer
        auto HD4k_media_lib_buffer = buffer->media_lib_buffers_list[MediaLibraryBufferType::Stream4K];

        if (!HD4k_media_lib_buffer) {
            std::cerr << "Buffer does not have 4K media library stream" << std::endl;
            return dsp_image_properties_t();
        }

        if (!HD4k_media_lib_buffer->hailo_pix_buffer) {
            std::cerr << "Failed to get hailo pix buffer" << std::endl;
            return dsp_image_properties_t();
        }

        return *HD4k_media_lib_buffer->hailo_pix_buffer.get();
    }

    void prepare_crops(BufferPtr input_buffer, std::vector<crop_resize_dims_extended_t> &crop_resize_dims) override
    {
        crop_resize_dims_extended_t resize_param;
        resize_param.perform_letterbox = true;
        resize_param.alignment = DSP_LETTERBOX_UP_LEFT;  //Only valid if perform_letterbox is set to true //DSP_LETTERBOX_UP_LEFT , DSP_LETTERBOX_MIDDLE
        resize_param.perform_crop = 0;  //Do not perform crop
        resize_param.crop_start_x = 0;  //Make sure to initialize the value to 0 even if crop is not performed
        resize_param.crop_start_y = 0;  //Make sure to initialize the value to 0 even if crop is not performed
        resize_param.crop_end_x = 0;    //Make sure to initialize the value to 0 even if crop is not performed
        resize_param.crop_end_y = 0;    //Make sure to initialize the value to 0 even if crop is not performed
        resize_param.destination_width = m_resized_width;
        resize_param.destination_height = m_resized_height;

        crop_resize_dims.push_back(resize_param);        
    }
};


class DetectorYolov5mPostProcessStage : public ProducableStage<BufferPtr, BufferPtr>
{
private:
    float   m_confidence_threshold =    0.6;
    int     m_total_class =             80;
    int     m_feature_map_size_1 =      20;
    int     m_feature_map_size_2 =      40;
    int     m_feature_map_size_3 =      80;

    std::vector<std::vector<BBox>>  m_bboxes;
    BufferPtr                       m_OutputConv93 = nullptr;
    BufferPtr                       m_OutputConv84 = nullptr;
    BufferPtr                       m_OutputConv74 = nullptr;
    std::string                     m_name;

bool endsWith(const std::string &str, const std::string &suffix) {
    if (str.length() >= suffix.length()) {
        return str.rfind(suffix) == (str.length() - suffix.length());
    }
    return false;
}
public:
    DetectorYolov5mPostProcessStage(std::string name, size_t queue_size) : 
        ProducableStage(name, queue_size, drop_buffer), m_name(name) { }

    int inline clamp(int val, int min, int max)
    {
        return std::max(min, std::min(max, val));
    }

    int process(BufferPtr data) override
    {

        std::string id = data->get_media_lib_buffer_id(MediaLibraryBufferType::Hailort);

        if (endsWith(id, "conv93")) {
            m_OutputConv93 = data;            
        } 
        else if (endsWith(id, "conv84")) { 
            m_OutputConv84 = data;
        }
        else if (endsWith(id, "conv74")) { 
            m_OutputConv74 = data;
        }

        if (m_OutputConv93 && m_OutputConv84 && m_OutputConv74) {
            
            static bool Initialized = false;
            static Yolov5NmsDecoder<uint8_t> Yolov5mDecoder(true);
            static std::vector<int> anchors1 {116, 90, 156, 198, 373, 326};
            static std::vector<int> anchors2 {30,  61, 62,  45,  59,  119};
            static std::vector<int> anchors3 {10,  13, 16,  30,  33,  23};

            if (Initialized == false) {
                
                //Config -  Note that we get the input width and height from "any" output (in this case we use m_OutputConv93)
                //          because all 3 output's metadata will contain the same information.
                CroppedBufferMetadataPtr ResizeCropMetadata = get_metadata<CroppedBufferMetadata>(m_OutputConv93, BufferMetadataType::Cropped);
                int NetworkInputWidth  = ResizeCropMetadata->destination_width;
                int NetworkInputHeight = ResizeCropMetadata->destination_height;

                Yolov5mDecoder.YoloConfig(NetworkInputWidth, NetworkInputHeight, m_total_class, m_confidence_threshold);

                QunatizationInfo quantInfo;

                //Get and set the quantization info of Conv93 which is output 20x20x255

                QuantInfoBufferMetadataPtr QuantInfoMetadata = get_metadata<QuantInfoBufferMetadata>(m_OutputConv93, BufferMetadataType::QuantizationInfo);
                quantInfo.qp_scale = QuantInfoMetadata->m_quant_info[0].qp_scale;
                quantInfo.qp_zp = QuantInfoMetadata->m_quant_info[0].qp_zp;
                Yolov5mDecoder.YoloAddOutput(m_feature_map_size_1, m_feature_map_size_1, anchors1, &quantInfo);

                //Get and set the quantization info of Conv84 which is output 40x40x255
                QuantInfoMetadata = get_metadata<QuantInfoBufferMetadata>(m_OutputConv84, BufferMetadataType::QuantizationInfo);
                quantInfo.qp_scale = QuantInfoMetadata->m_quant_info[0].qp_scale;
                quantInfo.qp_zp = QuantInfoMetadata->m_quant_info[0].qp_zp;
                Yolov5mDecoder.YoloAddOutput(m_feature_map_size_2, m_feature_map_size_2, anchors2, &quantInfo);

                //Get and set the quantization info of Conv74 which is output 80x80x255
                QuantInfoMetadata = get_metadata<QuantInfoBufferMetadata>(m_OutputConv74, BufferMetadataType::QuantizationInfo);
                quantInfo.qp_scale = QuantInfoMetadata->m_quant_info[0].qp_scale;
                quantInfo.qp_zp = QuantInfoMetadata->m_quant_info[0].qp_zp;
                Yolov5mDecoder.YoloAddOutput(m_feature_map_size_3, m_feature_map_size_3, anchors3, &quantInfo);

                Initialized = true;
            }

            std::vector<HailoDetectionPtr> Detections = Yolov5mDecoder.decode(  static_cast<uint8_t*>(m_OutputConv93->media_lib_buffers_list[MediaLibraryBufferType::Hailort]->get_plane(0)), 
                                                                                static_cast<uint8_t*>(m_OutputConv84->media_lib_buffers_list[MediaLibraryBufferType::Hailort]->get_plane(0)), 
                                                                                static_cast<uint8_t*>(m_OutputConv74->media_lib_buffers_list[MediaLibraryBufferType::Hailort]->get_plane(0)));
            

            CroppedBufferMetadataPtr cropped_buffer_metadata = get_metadata<CroppedBufferMetadata>(m_OutputConv93, BufferMetadataType::Cropped);
            if (!cropped_buffer_metadata) {
                std::cerr << "Failed to get cropped buffer metadata" << std::endl;
                return ERROR;
            }
            m_OutputConv93->remove_metadata(BufferMetadataType::Cropped);
            m_OutputConv93->remove_metadata(BufferMetadataType::QuantizationInfo);

            BufferPtr output_buffer = std::make_shared<Buffer>(false);
            output_buffer->copy_media_lib_buffers(m_OutputConv93);

            BBoxBufferMetadataPtr bbox_metadata = std::make_shared<BBoxBufferMetadata>( cropped_buffer_metadata->destination_width,
                                                                                        cropped_buffer_metadata->destination_height,
                                                                                        Detections,
                                                                                        cropped_buffer_metadata->letterbox);
            output_buffer->append_metadata(bbox_metadata);
            output_buffer->copy_metadata(m_OutputConv93);

            send_to_subscribers(output_buffer);

            //Clear the share pointer output buff after we are done extracting the bbox
            m_OutputConv93 = nullptr;
            m_OutputConv84 = nullptr;
            m_OutputConv74 = nullptr;
        }

        return SUCCESS;
    }
};


class DetectorDrawProcessStage : public ProducableStage<BufferPtr, BufferPtr>
{
private:
    std::string                     m_name;

public:
    DetectorDrawProcessStage(std::string name, size_t queue_size) : 
        ProducableStage(name, queue_size, drop_buffer), m_name(name) { }

    int process(BufferPtr data) override
    {
        BBoxBufferMetadataPtr bbox_buffer_metadata = get_metadata<BBoxBufferMetadata>(data, BufferMetadataType::BBox);
        if (!bbox_buffer_metadata) {
            std::cerr << "Failed to get bbox buffer metadata" << std::endl;
            return ERROR;
        }


        float scale_width_4K =  (float)data->media_lib_buffers_list[MediaLibraryBufferType::Stream4K]->hailo_pix_buffer->width / 
                                ((bbox_buffer_metadata->m_letterbox.is_performed) ?  (float)bbox_buffer_metadata->m_letterbox.width : (float)bbox_buffer_metadata->m_network_input_width);

        float scale_height_4K = (float)data->media_lib_buffers_list[MediaLibraryBufferType::Stream4K]->hailo_pix_buffer->height /
                                ((bbox_buffer_metadata->m_letterbox.is_performed) ?  (float)bbox_buffer_metadata->m_letterbox.height : (float)bbox_buffer_metadata->m_network_input_height);

        float scale_width_720p =    (float)data->media_lib_buffers_list[MediaLibraryBufferType::HD720p]->hailo_pix_buffer->width /
                                    ((bbox_buffer_metadata->m_letterbox.is_performed) ?  (float)bbox_buffer_metadata->m_letterbox.width : (float)bbox_buffer_metadata->m_network_input_width);

        float scale_height_720p =   (float)data->media_lib_buffers_list[MediaLibraryBufferType::HD720p]->hailo_pix_buffer->height /
                                    ((bbox_buffer_metadata->m_letterbox.is_performed) ?  (float)bbox_buffer_metadata->m_letterbox.height : (float)bbox_buffer_metadata->m_network_input_height);
        
        int line_thick = 8;
        H15NV12Draw NV12BoxDraw4K(data->media_lib_buffers_list[MediaLibraryBufferType::Stream4K], line_thick);
        line_thick = 2;
        H15NV12Draw NV12BoxDraw720p(data->media_lib_buffers_list[MediaLibraryBufferType::HD720p], line_thick);
        cv::Scalar color(0, 0, 255);
        
        //Must call draw_start before we draw anything on the buffer
        NV12BoxDraw4K.draw_start();
        NV12BoxDraw720p.draw_start();

        for (auto detection : bbox_buffer_metadata->m_bboxes) {

            //Get the confidence in string
            float confidence_in_percent = detection->get_confidence()*100.0;
            std::stringstream stream;
            stream << std::fixed << std::setprecision(1) << confidence_in_percent;
            std::string confidence_label = stream.str();
            //Get the final label with class name and confidence
            std::string label = Coco80Class_GetClassName(detection->get_class_id());
            label.append(" (").append(confidence_label).append("%)");

            float prediction_x = detection->get_bbox().xmin()*(float)bbox_buffer_metadata->m_network_input_width;
            float prediction_width = detection->get_bbox().width()*(float)bbox_buffer_metadata->m_network_input_width;

            float prediction_y = detection->get_bbox().ymin()*(float)bbox_buffer_metadata->m_network_input_height;
            float prediction_height = detection->get_bbox().height()*(float)bbox_buffer_metadata->m_network_input_height;

            float letterbox_correction_y = (bbox_buffer_metadata->m_letterbox.is_performed) ? bbox_buffer_metadata->m_letterbox.offset_y : 0.0;
            float letterbox_correction_x = (bbox_buffer_metadata->m_letterbox.is_performed) ? bbox_buffer_metadata->m_letterbox.offset_x : 0.0;

            int CorrectedBoxXPosition = (prediction_x - letterbox_correction_x);
            int CorrectedBoxYPosition = (prediction_y - letterbox_correction_y);

            cv::Rect rect_4k(   CorrectedBoxXPosition*scale_width_4K,
                                CorrectedBoxYPosition*scale_height_4K,
                                prediction_width*scale_width_4K,
                                prediction_height*scale_height_4K);

            NV12BoxDraw4K.draw_rectangle(rect_4k, color);

            NV12BoxDraw4K.draw_text(label,
                                    cv::Point(  CorrectedBoxXPosition*scale_width_4K, 
                                                CorrectedBoxYPosition*scale_height_4K - 10.0), 
                                    2.0, //Font scale
                                    4,   //Line thick
                                    color);

            cv::Rect rect_720p( CorrectedBoxXPosition*scale_width_720p,
                                CorrectedBoxYPosition*scale_height_720p,
                                prediction_width*scale_width_720p,
                                prediction_height*scale_height_720p);

            NV12BoxDraw720p.draw_rectangle(rect_720p, color);

            NV12BoxDraw720p.draw_text(  label,
                                        cv::Point(  CorrectedBoxXPosition*scale_width_720p, 
                                                    CorrectedBoxYPosition*scale_height_720p - 5.0), 
                                        0.75,  //Forn scale
                                        1,     //Line thick
                                        color);

        }

        //Must call draw_end after we are done drawing on the buffer
        NV12BoxDraw4K.draw_end();
        NV12BoxDraw720p.draw_end();

#if 0
        std::cout << "Total Detections: " << bbox_buffer_metadata->m_bboxes.size() << std::endl;
        for (auto detection : bbox_buffer_metadata->m_bboxes) {
            std::cout << "     Class: " << detection->get_class_id() << std::endl;
            std::cout << "     Class Name: " << Coco80Class_GetClassName(detection->get_class_id()) << std::endl;
            std::cout << "     Conf: " << detection->get_confidence() << std::endl;
            std::cout << "     xmin: " << detection->get_bbox().xmin() << std::endl;
            std::cout << "     ymin: " << detection->get_bbox().ymin() << std::endl;
            std::cout << "     width: " << detection->get_bbox().width() << std::endl;
            std::cout << "     Height: " << detection->get_bbox().height() << std::endl;

        }
#endif

        data->remove_metadata(BufferMetadataType::BBox);
        send_to_subscribers(data);

        return SUCCESS;
    }
};


class EncodeStreamingProcessStage : public ProducableStage<BufferPtr, BufferPtr>
{
private:
    std::string                     m_name;
    std::map<output_stream_id_t, MediaLibraryEncoderPtr> *m_encoders;

public:
    EncodeStreamingProcessStage(std::string name, std::map<output_stream_id_t, MediaLibraryEncoderPtr> *encoders, size_t queue_size) : 
        ProducableStage(name, queue_size, drop_buffer), m_name(name), m_encoders(encoders) { }

    int process(BufferPtr data) override
    {
        for (auto& [key, encoder] : *m_encoders)
        {
            if (key.compare("sink0") == 0) {
                encoder->add_buffer(data->media_lib_buffers_list[MediaLibraryBufferType::Stream4K]);
            }

            if (key.compare("sink1") == 0) {
                encoder->add_buffer(data->media_lib_buffers_list[MediaLibraryBufferType::HD720p]);
            }
        }

        return SUCCESS;
    }
};


class FrontendAggregatorStage : public ProducableBufferStage
{
private:
    SmartQueue<BufferPtr> m_4k_queue;
    SmartQueue<BufferPtr> m_fhd_queue;
public:
    FrontendAggregatorStage(std::string name, size_t queue_size) : 
        ProducableBufferStage(name, queue_size, false),
        m_4k_queue("4k_agg_queue", 5, drop_buffer, false),
        m_fhd_queue("fhd_agg_queue", 5, drop_buffer, false) { }
 
    int process(BufferPtr data) override
    {
        if (data->has_Bufferkey(MediaLibraryBufferType::Stream4K))
        {
            m_4k_queue.push(data);
        }
        else if (data->has_Bufferkey(MediaLibraryBufferType::HD720p))
        {
            m_fhd_queue.push(data);
        }
        else
        {
            std::cout << "WARNING: FrontendAggregatorStage unable to find required stream key" << std::endl;
            return ERROR;
        }

        bool both_queues_has_buffer = m_4k_queue.size() > 0 && m_fhd_queue.size() > 0;

        if (!both_queues_has_buffer)
        {
            return SUCCESS;
        }

        BufferPtr output_buffer = std::make_shared<Buffer>(false);

        output_buffer->copy_media_lib_buffers(m_4k_queue.pop());
        output_buffer->copy_media_lib_buffers(m_fhd_queue.pop());

        send_to_subscribers(output_buffer);

        return SUCCESS;
    }
};
