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

#ifndef _H15Common_H_
#define _H15Common_H_

#include <time.h>
#include <ctype.h>
#include <cstring> 
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <thread>
#include <atomic>
#include <signal.h>
#include <mutex>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <functional>
#include <memory>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspconnection.h>

typedef enum eEncoding {

    H265                    = 0,
    H264                    = 1,
    
} eEncoding;

typedef enum eVideoFormat {

    VIDEO_FORMAT_NV12                    = 0,    //Recommended and currently its the only format supported by H15 encoder
    //VIDEO_FORMAT_RGB                   = 1,    //NOT supported YET
} eVideoFormat;

typedef enum eStreamOutSource {

    SOURCE_OUT_UDP                     = 1,
    SOURCE_OUT_RTSP                    = 2,

} eStreamOutSource;


typedef struct H15EncoderConfig
{
    eEncoding           Encoding = H265;
    eVideoFormat        VideoFormat = VIDEO_FORMAT_NV12;

    int                 width = 1920;                       //Up to 3840, Will NOT auto resize, MUST match to the feeding frame from feedFrame
    int                 height = 1080;                      //Up to 2160, Will NOT auto resize, MUST match to the feeding frame from feedFrame
    int                 desiredFPS = 30; 
    int                 bitrate = 20000000;                 //Bitrate in bps

} H15EncoderConfig;

typedef struct H15StreamConfig
{
    H15EncoderConfig    encodingConfig;

    eStreamOutSource    streamOutSource = SOURCE_OUT_RTSP;

    //If streamOutSource is FILE
    std::string         streamOutFile   = "output.h26x";      //File name to save the stream, ONLY valid if streamOutSource is FILE

    //If streamOutSource is UDP / RTSP
    std::string         streamOutIP     = "10.0.0.1";         //IP address to send the stream
    int                 streamOutPort   = 5000;               //Port to send the stream

    //If streamOutSource is RTSP
    std::string         streamOutRTSPPath = "/H15RtspStream";          //RTSP path to send the stream

} H15StreamConfig;


#endif /* _H15Common_H_ */
