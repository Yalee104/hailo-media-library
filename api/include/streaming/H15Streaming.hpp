/**
 * Copyright (c) 2020-2023 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/

/* 
 * File:   H15StreamEncoding.hpp
 * Author: Aaron Lee
 * 
 * Created on Sept 28, 2023, 11:32 AM
 */

#ifndef _H15STREAMING_H_
#define _H15STREAMING_H_

#include "H15Common.hpp"

typedef enum eStreamRetCode {

    STREAM_SUCCESS                    = 0,
    
    STREAM_UNINITIALIZED              = -1,
    STREAM_INVALID_CONFIG_PARAM       = -2,
    STREAM_FAILED                     = -3,
    STREAM_NOT_RUNNING                = -4, 

} eStreamRetCode;

enum eStreamingState { 
    STREAMING_STATE_UNINITIALIZED, 
    STREAMING_STATE_INITIALIZED, 
    STREAMING_STATE_RUNNING, 
    STREAMING_STATE_STOPPED 
};

struct GstStreamingPipelineData {

    GstElement*         pipelineStream = nullptr;
    GstElement*         srcStream = nullptr;
    guint64             m_Pts = 0;

    H15StreamConfig     streamingConfig;
};

class H15Streaming;
using H15StreamingPtr = std::shared_ptr<H15Streaming>;


class FrameRateCalculator {
private:
    int frameCount;
    std::chrono::time_point<std::chrono::steady_clock> startTime;

public:
    FrameRateCalculator() : frameCount(0), startTime(std::chrono::steady_clock::now()) {}

    void reset() {
        std::cout << "FrameRateCalculation Reset" << std::endl;

        startTime = std::chrono::steady_clock::now();
        frameCount = 0;
    }

    void receivedFrame() {
        frameCount++;
    }

    int getAccumulatedFrame() {
        return frameCount;
    }

    float getFPS() {

        float CalculatedFps = 0;
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();

        if (duration > 0) {
            CalculatedFps = (static_cast<float>(frameCount) /  (static_cast<float>(duration) / 1000.0 )) * 1000.0;
            std::cout << "Actual FPS: " << CalculatedFps  << ", FPS (INT): " << (int)(CalculatedFps) << std::endl;
        }

        return CalculatedFps;
    }

};


class H15Streaming
{

public:
    eStreamingState             m_state = STREAMING_STATE_UNINITIALIZED;    
    GstStreamingPipelineData    m_gstPipelineData;
    int                         m_numClientConnected = 0;

private:
    std::stringstream   m_gstPipelineStream;
    GMainLoop*          m_RtspStreamloop = nullptr;
    std::thread         m_RtspServerThread;
    FrameRateCalculator m_FpsCalculator;

public:

    /**
     * Constructor
     * @param NONE
     * @return NONE
     */
    H15Streaming();
    ~H15Streaming();

    /**
     * Config the stream encoding pipeline
     * NOTE1: If set to UDP streaming use below gstreamer pipeline on recieving system end to play the video
     *          * gst-launch-1.0 -v udpsrc port=5000 ! application/x-rtp,encoding-name=H264 ! queue ! rtph264depay ! 
     *            queue ! h264parse ! avdec_h264 ! queue ! videoconvert n-threads=4 ! fpsdisplaysink text-overlay=false sync=false
     * NOTE2: If set to RTSP please make sure the IP is set to the same IP of the H15 system (default is 10.0.0.1).
     * @param config	Config the stream pipeline
     * @return eStreamRetCode
     */
    eStreamRetCode Config(H15StreamConfig &config);

    /**
     * Start the pipeline
     * @param NONE
     * @return eStreamRetCode
     */
    eStreamRetCode start();

    /**
     * Feed the frame for encoding data
     * @param pFrameBuffer    The frame buffer to feed
     * @return eStreamRetCode
     */
    eStreamRetCode feedData(GstBuffer* buffer);

    /**
     * Feed the frame for encoding data
     * @param buffer    The encoded data
     * @param size      The size of the encoded data
     * @return eStreamRetCode
     */
    eStreamRetCode feedData(char* buffer, uint32_t size);

    /**
     * Update the encoder info should it change during runtime
     * @param config    The encoder config to update
     * @return eStreamRetCode
     */
    eStreamRetCode UpdateEncodingInfo(H15EncoderConfig &config);


    /**
     * WARNING: This is a temporary solution ONLY. We should get the encoder info from the media library encoder
     *          parameter API instead when its available and NOT from the JSON file.
     * Helper function to parse necessary information from JSON file
     * @param jsonEncodingInfo  The json string to parse    
     * @return H15EncoderConfig The encoder config information returned
     */
    H15EncoderConfig GetEncodingInfo(std::string &jsonEncodingInfo);


private:
    
    eStreamRetCode CreateRtspStreamPipeline(H15StreamConfig &config);

    eStreamRetCode CreateUdpStreamPipeline(H15StreamConfig &config);
    
    eStreamRetCode SetGstBufferTime(GstBuffer* gstbuffer);

};


#endif /* _H15STREAMING_H_ */
