#include "buffer_utils.hpp"
#include "media_library/encoder.hpp"
#include "media_library/frontend.hpp"
#include "infra/stages.hpp"
#include "infra/dsp_stages.hpp"
#include "infra/hailort_stage.hpp"
#include "infra/pipeline.hpp"
#include "user_stages.hpp"

#include <queue>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <tl/expected.hpp>
#include "streaming/H15Streaming.hpp"

#define USE_RTSP_STREAMING  1

#define FRONTEND_CONFIG_FILE "frontend_native_config_example.json"
#define ENCODER_OSD_CONFIG_FILE(id) get_encoder_osd_config_file(id)
#define OUTPUT_FILE(id) get_output_file(id)
#define RUNTIME_SECONDS (60*60)


#define DETECTOR_HEF_FILE ("resources/yolov5m_wo_spp_nv12.hef")
#define DETECTOR_BATCH_SIZE 1
#define DETECTOR_WIDTH  640
#define DETECTOR_HEIGHT 640

#define DETECTOR_YOLOV5S_HEF_FILE ("resources/yolov5s_nv12.hef")
#define DETECTOR_YOLOV5S_BATCH_SIZE 1
#define DETECTOR_YOLOV5S_WIDTH  640
#define DETECTOR_YOLOV5S_HEIGHT 640


inline std::string get_encoder_osd_config_file(const std::string &id)
{
    return "frontend_encoder_" + id + ".json";
}

inline std::string get_output_file(const std::string &id)
{
    return "frontend_example_" + id + ".h264";
}

void stream_encoded_data(HailoMediaLibraryBufferPtr buffer, uint32_t size, H15StreamingPtr streaming, std::string &json_encode_param)
{
    char *data = (char *)buffer->get_plane(0);
    if (!data)
    {
        std::cout << "Error occurred at streaming time!" << std::endl;
        return;
    }
    
    if (streaming)
        streaming->feedData(data, size);
}

void write_encoded_data(HailoMediaLibraryBufferPtr buffer, uint32_t size, std::ofstream &output_file)
{
    char *data = (char *)buffer->get_plane(0);
    if (!data)
    {
        std::cout << "Error occurred at writing time!" << std::endl;
        return;
    }
    output_file.write(data, size);
}

std::string read_string_from_file(const char *file_path)
{
    std::ifstream file_to_read;
    file_to_read.open(file_path);
    if (!file_to_read.is_open())
        throw std::runtime_error("config path is not valid");
    std::string file_string((std::istreambuf_iterator<char>(file_to_read)),
                            std::istreambuf_iterator<char>());
    file_to_read.close();
    std::cout << "Read config from file: " << file_path << std::endl;
    return file_string;
}

void delete_output_file(std::string output_file)
{
    std::ofstream fp(output_file.c_str(), std::ios::out | std::ios::binary);
    if (!fp.good())
    {
        std::cout << "Error occurred at writing time!" << std::endl;
        return;
    }
    fp.close();
}

struct AppResources
{
    MediaLibraryFrontendPtr frontend;
    std::map<output_stream_id_t, MediaLibraryEncoderPtr> encoders;
    std::map<output_stream_id_t, std::ofstream> output_files;
    std::unique_ptr<Pipeline> pipeline;
    std::shared_ptr<ProducableBufferStage> source_stage;
    std::map<output_stream_id_t, H15StreamingPtr> streamings;
    std::map<output_stream_id_t, std::string> json_encoder_param;
};

void subscribe_elements(std::shared_ptr<AppResources> app_resources)
{
    auto streams = app_resources->frontend->get_outputs_streams();
    if (!streams.has_value())
    {
        std::cout << "Failed to get stream ids" << std::endl;
        throw std::runtime_error("Failed to get stream ids");
    }

    FrontendCallbacksMap fe_callbacks;

    for (auto s : streams.value())
    {
        std::cout << "subscribing to frontend for '" << s.id << "'" << std::endl;

        fe_callbacks[s.id] = [s, app_resources](HailoMediaLibraryBufferPtr media_lib_buffer, size_t size)
        {
            int width = media_lib_buffer->hailo_pix_buffer->width;
            int height = media_lib_buffer->hailo_pix_buffer->height;

            if (width == 3840 && height == 2160)
            {
                BufferPtr buffer = std::make_shared<Buffer>(false);
                buffer->add_media_lib_buffer(MediaLibraryBufferType::Stream4K, std::move(media_lib_buffer));
                app_resources->source_stage->push(buffer);
            }
            else if (width == 1280 && height == 720)
            {
                BufferPtr buffer = std::make_shared<Buffer>(false);
                buffer->add_media_lib_buffer(MediaLibraryBufferType::HD720p, std::move(media_lib_buffer));
                app_resources->source_stage->push(buffer);
            }
            else
            {
                std::cout << "WARNING: Frontend stream " << width << "X" << height << " not handled" << std::endl;
            }
        };
    }
    app_resources->frontend->subscribe(fe_callbacks);

    for (const auto &entry : app_resources->encoders)
    {
        output_stream_id_t streamId = entry.first;
        MediaLibraryEncoderPtr encoder = entry.second;
        std::cout << "subscribing to encoder for '" << streamId << "'" << std::endl;
        app_resources->encoders[streamId]->subscribe(
            [app_resources, streamId](HailoMediaLibraryBufferPtr buffer, size_t size)
            {
                stream_encoded_data(buffer, size, app_resources->streamings[streamId], app_resources->json_encoder_param[streamId]);
                //write_encoded_data(buffer, size, app_resources->output_files[streamId]);
                buffer->decrease_ref_count();
            });
    }
}

void create_encoder_and_output_file(const std::string& id, std::shared_ptr<AppResources> app_resources)
{

    std::cout << "Creating encoder enc_" << id << std::endl;

    // Create and configure encoder
    std::string encoderosd_config_string = read_string_from_file(ENCODER_OSD_CONFIG_FILE(id).c_str());
    tl::expected<MediaLibraryEncoderPtr, media_library_return> encoder_expected = MediaLibraryEncoder::create(encoderosd_config_string, id);
    if (!encoder_expected.has_value())
    {
        std::cout << "Failed to create encoder osd" << std::endl;
        return;
    }
    app_resources->encoders[id] = encoder_expected.value();

    // create and configure output file
    std::string output_file_path = OUTPUT_FILE(id);
    delete_output_file(output_file_path);
    app_resources->output_files[id].open(output_file_path.c_str(), std::ios::out | std::ios::binary | std::ios::app);
    if (!app_resources->output_files[id].good())
    {
        std::cout << "Error occurred at writing time!" << std::endl;
        return;
    }

    // Add Json encoder param
    app_resources->json_encoder_param[id] = encoderosd_config_string;

}


void create_and_config_streaming(const std::string& id, std::shared_ptr<AppResources> app_resources)
{
    // For each RTSP stream we need to assign different port number
    static int portNum = 5000;

    app_resources->streamings[id] = std::make_shared<H15Streaming>();
    H15StreamConfig streamConfig;
#if USE_RTSP_STREAMING        
    streamConfig.streamOutSource = SOURCE_OUT_RTSP;
    streamConfig.streamOutRTSPPath = "/H15RtspStream_" + id;
    streamConfig.streamOutIP = "10.0.0.1";
#else
    streamConfig.streamOutSource = SOURCE_OUT_UDP;
    streamConfig.streamOutIP = "10.0.0.2";
#endif
    streamConfig.streamOutPort = portNum;
    streamConfig.encodingConfig = app_resources->streamings[id]->GetEncodingInfo(app_resources->json_encoder_param[id]);
    app_resources->streamings[id]->Config(streamConfig);
    app_resources->streamings[id]->start();
    portNum += 2;  //Increase the port Number for next stream
}


void stop(std::shared_ptr<AppResources> app_resources)
{
    std::cout << "Stopping." << std::endl;
    app_resources->frontend->stop();
    for (const auto &entry : app_resources->encoders)
    {
        entry.second->stop();
    }

    // close all file in media_lib->output_files
    for (auto &entry : app_resources->output_files)
    {
        entry.second.close();
    }

    app_resources->pipeline->stop_pipeline();

    //Release all the streaming
    app_resources->streamings.clear();

}

void configure_frontend(std::shared_ptr<AppResources> app_resources)
{
    std::string frontend_config_string = read_string_from_file(FRONTEND_CONFIG_FILE);
    tl::expected<MediaLibraryFrontendPtr, media_library_return> frontend_expected = MediaLibraryFrontend::create(FRONTEND_SRC_ELEMENT_V4L2SRC, frontend_config_string);
    if (!frontend_expected.has_value())
    {
        std::cout << "Failed to create frontend" << std::endl;
        return;
    }
    app_resources->frontend = frontend_expected.value();

    auto streams = app_resources->frontend->get_outputs_streams();
    if (!streams.has_value())
    {
        std::cout << "Failed to get stream ids" << std::endl;
        throw std::runtime_error("Failed to get stream ids");
    }

    for (auto s : streams.value())
    {
        create_encoder_and_output_file(s.id, app_resources);
        create_and_config_streaming(s.id, app_resources);
    }
}

void start_frontend(std::shared_ptr<AppResources> app_resources)
{
    for (const auto &entry : app_resources->encoders)
    {
        output_stream_id_t streamId = entry.first;
        MediaLibraryEncoderPtr encoder = entry.second;

        std::cout << "starting encoder for " << streamId << std::endl;
        encoder->start();
    }

    app_resources->pipeline->run_pipeline();
    sleep(1);
    app_resources->frontend->start();
}


void create_pipeline(std::shared_ptr<AppResources> app_resources)
{
    app_resources->pipeline = std::make_unique<Pipeline>();

    std::shared_ptr<FrontendAggregatorStage> frontend_aggregator_stage = std::make_shared<FrontendAggregatorStage>("frontend_agg", 5);

    std::shared_ptr<DetectorResizeStage> detector_resize_stage = std::make_shared<DetectorResizeStage>("detector_resize", 
                                                                                                        5, 
                                                                                                        10, 
                                                                                                        DETECTOR_WIDTH, 
                                                                                                        DETECTOR_HEIGHT);

    //WARNING: Make sure the device group id is the same from hailort:device-id in frontend_native_config_example.json
    std::string hailort_dev_group_id = "device0";
    std::shared_ptr<HailortAsyncStage> detector_infer_stage = std::make_shared<HailortAsyncStage>(  "hrt_detector", 
                                                                                                    10, 
                                                                                                    20, 
                                                                                                    DETECTOR_HEF_FILE, 
                                                                                                    hailort_dev_group_id, 
                                                                                                    DETECTOR_BATCH_SIZE);

    std::shared_ptr<DetectorYolov5mPostProcessStage> detector_post_process_stage = std::make_shared<DetectorYolov5mPostProcessStage>("detector_post_process", 
                                                                                                                                      5 * DETECTOR_BATCH_SIZE);

    std::shared_ptr<EncodeStreamingProcessStage> encode_stream_process_stage = std::make_shared<EncodeStreamingProcessStage>("enc_stream_process", 
                                                                                                                             &app_resources->encoders, 
                                                                                                                             10);

    std::shared_ptr<DetectorDrawProcessStage> detector_draw_process_stage = std::make_shared<DetectorDrawProcessStage>("det_draw_process", 
                                                                                                                        10);

    app_resources->source_stage = std::static_pointer_cast<ProducableBufferStage>(frontend_aggregator_stage);
    
    app_resources->pipeline->add_stage(frontend_aggregator_stage);
    app_resources->pipeline->add_stage(detector_resize_stage);
    app_resources->pipeline->add_stage(detector_infer_stage);
    app_resources->pipeline->add_stage(detector_post_process_stage);
    app_resources->pipeline->add_stage(encode_stream_process_stage);
    app_resources->pipeline->add_stage(detector_draw_process_stage);
    
    //Pipeline Flow - After we get the frontend stream(s) we pass it to the detector resize stage to resize to detector network input size
    frontend_aggregator_stage->add_subscriber(detector_resize_stage);

    //Pipeline Flow - After we resize it we pass in to the detector inference 
    detector_resize_stage->add_subscriber(detector_infer_stage);

    //Pipeline Flow - After we infer the resized frame to the network we pass it to the detector post process
    detector_infer_stage->add_subscriber(detector_post_process_stage);

    //Pipeline Flow - After we are done with post-process we pass it to the detector draw process
    detector_post_process_stage->add_subscriber(detector_draw_process_stage);

    //Pipeline Flow - After we are done drawing the bbox we pass it to the encoder+stream and this will be the final stage
    detector_draw_process_stage->add_subscriber(encode_stream_process_stage);

}

#include "media_library/signal_utils.hpp"
std::shared_ptr<AppResources> app_resources;

int main(int argc, char *argv[])
{
    app_resources = std::make_shared<AppResources>();

    // register signal SIGINT and signal handler  
    signal_utils::register_signal_handler([](int signal)
                                        {
                                        std::cout << "Stopping Pipeline..." << std::endl;
                                        
                                        stop(app_resources);
                                        
                                        // terminate program  
                                        exit(signal);; });

    create_pipeline(app_resources);
    configure_frontend(app_resources);
    subscribe_elements(app_resources);
    start_frontend(app_resources);

    std::cout << "Started playing for " << RUNTIME_SECONDS << " seconds." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(RUNTIME_SECONDS));

    stop(app_resources);

    return 0;
}
