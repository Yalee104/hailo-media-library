/**
 * Copyright (c) 2020-2023 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/

/* 
 * File:   H15Streaming.hpp
 * Author: Aaron Lee
 * 
 * Created on Sept 28, 2023, 11:32 AM
 */

#include "H15Streaming.hpp"
#include <nlohmann/json.hpp>
//#include <gst/codecparsers/gsth264parser.h>   //From plugins bad

/* Static Functions ====================================================================================================================== */


void UpdateStreamingCaps(H15EncoderConfig &config, GstElement* srcElement)
{
	if (srcElement) {
		std::string videoType = "video/x-h264";
		if (config.Encoding == H265)
			videoType = "video/x-h265";

		GstCaps* caps = gst_caps_new_simple (videoType.c_str(),
			"stream-format", G_TYPE_STRING, "byte-stream",
			"alignment", G_TYPE_STRING, "au",
			"width", G_TYPE_INT, config.width , "height", G_TYPE_INT, config.height,
			"framerate", GST_TYPE_FRACTION, config.desiredFPS, 1, NULL);
		gst_util_set_object_arg (G_OBJECT (srcElement), "format", "time");
		g_object_set (G_OBJECT (srcElement), "caps", caps, NULL);

		std::cout << "UpdateStreamingCaps : " << gst_caps_to_string(caps) << std::endl;

		gst_caps_unref (caps);

	}
}

static void
media_config_free_resource (H15Streaming* pObj)
{
	//TODO: Add mutex to protect access to m_gstPipelineData.srcStream to prevent possible conflict with feedData

  	g_print ("encodepipeline_free (disconnected or media player stoped)\n");
	gst_object_unref(pObj->m_gstPipelineData.srcStream);
	pObj->m_gstPipelineData.srcStream = nullptr;
}

static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
	std::cout << "media_configure" << std::endl;
	
	H15Streaming *pObj = (H15Streaming *)user_data;

	pObj->m_gstPipelineData.m_Pts = 0;

  	GstElement* element = gst_rtsp_media_get_element (media);

	pObj->m_gstPipelineData.pipelineStream = element;
	pObj->m_gstPipelineData.srcStream = gst_bin_get_by_name_recurse_up (GST_BIN (element), "streamdatasrc");

  	/* make sure the data is freed when the media is gone */
  	g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", pObj,  (GDestroyNotify) media_config_free_resource);

	UpdateStreamingCaps(pObj->m_gstPipelineData.streamingConfig.encodingConfig, pObj->m_gstPipelineData.srcStream);
}


void client_disconnected (GstRTSPClient *client, gpointer user_data)
{
	H15Streaming *pObj = (H15Streaming *)user_data;

  	GstRTSPConnection *connection;
  	connection = gst_rtsp_client_get_connection(client);
  	g_print("Client disconnected!!: %s\n", gst_rtsp_connection_get_ip(connection));

	pObj->m_numClientConnected--;
}

void client_connected (GstRTSPServer *server, GstRTSPClient *client, gpointer user_data) 
{

	H15Streaming *pObj = (H15Streaming *)user_data;

  	GstRTSPConnection *connection;
  	connection = gst_rtsp_client_get_connection(client);
  	g_print("Client connected: %s\n", gst_rtsp_connection_get_ip(connection));

	pObj->m_numClientConnected++;

  	/* connect to the client-disconnected signal */
  	g_signal_connect(client, "closed", (GCallback)client_disconnected, user_data);

}



/* Class Method Implementation =============================================================================================================== */


H15Streaming::H15Streaming()
{
	// Init gstreamer if not initialized before
    gst_init(NULL, NULL);

	m_state = STREAMING_STATE_UNINITIALIZED;
}

H15Streaming::~H15Streaming()
{
	//Terminate the Rtsp server loop
	//std::cout << "H15Streaming::~H15Streaming" << std::endl;

	if (m_RtspStreamloop != nullptr) {
		g_main_loop_quit(m_RtspStreamloop);
		if (m_RtspServerThread.joinable())
            m_RtspServerThread.join();
	}

	if (m_gstPipelineData.srcStream)
		gst_object_unref(m_gstPipelineData.srcStream);
	
	if (m_gstPipelineData.pipelineStream)
		gst_object_unref(m_gstPipelineData.pipelineStream);
}

eStreamRetCode H15Streaming::UpdateEncodingInfo(H15EncoderConfig &config)
{
	eStreamRetCode returnCode = STREAM_SUCCESS;

	if (m_state == eStreamingState::STREAMING_STATE_UNINITIALIZED) {
		return eStreamRetCode::STREAM_UNINITIALIZED;
	}

	m_gstPipelineData.streamingConfig.encodingConfig = config;

	if (m_numClientConnected) {
		UpdateStreamingCaps(m_gstPipelineData.streamingConfig.encodingConfig, m_gstPipelineData.srcStream);
	}

	return returnCode;
}


eStreamRetCode H15Streaming::CreateRtspStreamPipeline(H15StreamConfig &config)
{

    // Create GSTreamer pipeline
	std::string 	  H15EncoderParse = "h265parse";
	std::string 	  H15EncoderParseType = "video/x-h265";
	std::string 	  H15RtphPayload = "rtph265pay";
	std::string 	  queueBuffSize = "5";

	m_gstPipelineStream.str("");
	m_gstPipelineStream.clear();

	if (config.encodingConfig.Encoding == H264) {
		H15EncoderParse = "h264parse";
		H15EncoderParseType = "video/x-h264";
		H15RtphPayload = "rtph264pay";
	}

    m_gstPipelineStream << "( appsrc name=streamdatasrc ! ";
	m_gstPipelineStream << H15RtphPayload << " name=pay0 pt=96 )";

	//Show the pipeline
	std::cout << "GstPipeline RtspStream : " << m_gstPipelineStream.str() << std::endl;
	
	/* Gstreamer RTSP taken from https://github.com/GStreamer/gst-rtsp-server/tree/master/examples */
    m_RtspServerThread = std::thread([this]() -> void {

		GstRTSPServer *server;
		GstRTSPMountPoints *mounts;
		GstRTSPMediaFactory *factory;

	  	m_RtspStreamloop = g_main_loop_new (NULL, FALSE);

		server = gst_rtsp_server_new ();
		if (server == NULL) {
			g_printerr ("Failed to create RTSP server\\n");
		}

		//IP Address
		g_object_set(server, "address", this->m_gstPipelineData.streamingConfig.streamOutIP.c_str(), NULL);	

		//Port
		g_object_set(server, "service", std::to_string(this->m_gstPipelineData.streamingConfig.streamOutPort).c_str(), NULL);

		mounts = gst_rtsp_server_get_mount_points (server);
		if (mounts == NULL) {
			g_printerr ("Failed to create RTSP mounts\\n");
		}

		factory = gst_rtsp_media_factory_new ();
		if (factory == NULL) {
			g_printerr ("Failed to create RTSP factory\\n");
		}

		gst_rtsp_media_factory_set_launch (factory, this->m_gstPipelineStream.str().c_str());

		//Allow multiple client connection to share with the same stream content
		gst_rtsp_media_factory_set_shared(factory, TRUE);

		gst_rtsp_media_factory_set_enable_rtcp (factory, TRUE);

		g_signal_connect (factory, "media-configure", (GCallback) media_configure, this);

		/* connect to the client-connected signal */
		g_signal_connect(server, "client-connected", (GCallback)client_connected, this);

		gst_rtsp_mount_points_add_factory (mounts, this->m_gstPipelineData.streamingConfig.streamOutRTSPPath.c_str() , factory);

		g_object_unref (mounts);

		if (gst_rtsp_server_attach (server, NULL) == 0) {
			g_printerr ("Failed to attached server\\n");
			//TODO: We need error handling here to notify H15StreamEncoding that we failed to launch server!!!!
		}

		/* start serving */
		std::cout << "stream ready at rtsp://" 	<< this->m_gstPipelineData.streamingConfig.streamOutIP.c_str() 
												<< ":" 
												<< std::to_string(this->m_gstPipelineData.streamingConfig.streamOutPort).c_str() 
												<< this->m_gstPipelineData.streamingConfig.streamOutRTSPPath.c_str() << std::endl;

		this->m_state = eStreamingState::STREAMING_STATE_RUNNING;

	  	g_main_loop_run (m_RtspStreamloop);

		g_main_loop_unref(m_RtspStreamloop); 
		
		//g_print ("RTSP Server exiting\n");

    });

	return eStreamRetCode::STREAM_SUCCESS;
}


eStreamRetCode H15Streaming::CreateUdpStreamPipeline(H15StreamConfig &config)
{
    // Create GSTreamer pipeline
	std::string 	  H15EncoderParse = "h265parse";
	std::string 	  H15EncoderParseType = "video/x-h265";
	std::string 	  H15RtphPayload = "rtph265pay";
	std::string 	  queueBuffSize = "5";
	std::string 	  encodingName = "H265";

	m_gstPipelineStream.str("");
	m_gstPipelineStream.clear();

	if (config.encodingConfig.Encoding == H264) {
		H15EncoderParse = "h264parse";
		H15EncoderParseType = "video/x-h264";
		H15RtphPayload = "rtph264pay";
		encodingName = "H264";
	}

    m_gstPipelineStream << "appsrc name=streamdatasrc ! ";
	m_gstPipelineStream << H15EncoderParse << " config-interval=1 ! ";
	m_gstPipelineStream << H15EncoderParseType << ",framerate=" << config.encodingConfig.desiredFPS << "/1 ! ";
	m_gstPipelineStream << "queue leaky=upstream " << "max-size-buffers=" << queueBuffSize << " max-size-bytes=0 max-size-time=0 ! ";
	m_gstPipelineStream << H15RtphPayload << " ! " << "capsfilter caps=\"application/x-rtp,media=(string)video,encoding-name=(string)";
	m_gstPipelineStream << encodingName << "\"" << " ! ";
	m_gstPipelineStream << "udpsink host=" << config.streamOutIP << " port=" << config.streamOutPort << " name=udp_sink sync=false";

	//Show the pipeline
	std::cout << "GstPipeline UdpStream : " << m_gstPipelineStream.str() << std::endl;

	GError *err = nullptr;
    m_gstPipelineData.pipelineStream = gst_parse_launch(m_gstPipelineStream.str().c_str(), &err);
	if (err) {
		std::cerr << "GstPipeline Check Err : " << err->message << std::endl;
		return eStreamRetCode::STREAM_FAILED;
	}

	// Find our appsrc by name
    m_gstPipelineData.srcStream = gst_bin_get_by_name(GST_BIN(m_gstPipelineData.pipelineStream), "streamdatasrc");

	// Set Caps
	UpdateStreamingCaps(config.encodingConfig, m_gstPipelineData.srcStream);

	//gst_element_set_state(m_gstPipelineData.pipelineStream, GST_STATE_PLAYING);
    gst_element_set_state(m_gstPipelineData.pipelineStream, GST_STATE_PLAYING);

	m_state = eStreamingState::STREAMING_STATE_RUNNING;

	m_numClientConnected = 1; //Client always connected for UDP

	return eStreamRetCode::STREAM_SUCCESS;
}


eStreamRetCode H15Streaming::Config(H15StreamConfig &config)
{
	if (m_state != eStreamingState::STREAMING_STATE_UNINITIALIZED) {
		return eStreamRetCode::STREAM_INVALID_CONFIG_PARAM;
	}

	m_gstPipelineData.streamingConfig = config;

	m_state = eStreamingState::STREAMING_STATE_INITIALIZED;

	return eStreamRetCode::STREAM_SUCCESS;
}


eStreamRetCode H15Streaming::start()
{
	if (m_state == eStreamingState::STREAMING_STATE_UNINITIALIZED) {
		return eStreamRetCode::STREAM_UNINITIALIZED;
	}

	if (m_gstPipelineData.streamingConfig.streamOutSource == eStreamOutSource::SOURCE_OUT_RTSP) {
		CreateRtspStreamPipeline(m_gstPipelineData.streamingConfig);
	}
	else if (m_gstPipelineData.streamingConfig.streamOutSource == eStreamOutSource::SOURCE_OUT_UDP) {
		CreateUdpStreamPipeline(m_gstPipelineData.streamingConfig);
	}
	else {
		std::cout << "WARNING: Unsupported Stream Out Source : " << m_gstPipelineData.streamingConfig.streamOutSource << std::endl;
		return eStreamRetCode::STREAM_INVALID_CONFIG_PARAM;
	}

	return eStreamRetCode::STREAM_SUCCESS;
}


eStreamRetCode H15Streaming::feedData(GstBuffer* buffer)
{
	eStreamRetCode returnCode = STREAM_SUCCESS;

	if (m_state != eStreamingState::STREAMING_STATE_RUNNING) {
		return eStreamRetCode::STREAM_NOT_RUNNING;
	}
	
	//Just don't send anything if there is no client(s) connected or pipeline is not ready (because of client not yet connected)
	if (m_numClientConnected <= 0 || m_gstPipelineData.srcStream == nullptr) {
		return eStreamRetCode::STREAM_SUCCESS;
	}

#if 0
	GstH264NalParser *parser;
	GstH264NalUnit nalu;
	GstMapInfo map;

	/* Map the buffer */
	gst_buffer_map(buffer, &map, GST_MAP_READ);

	/* Iterate over the buffer data */
	for (gsize i = 0; i < map.size - 4; i++) {
		/* Look for the start code (0x000001 or 0x00000001) */
		if ((map.data[i] == 0x00 && map.data[i+1] == 0x00 && map.data[i+2] == 0x01) ||
			(map.data[i] == 0x00 && map.data[i+1] == 0x00 && map.data[i+2] == 0x00 && map.data[i+3] == 0x01)) {
			/* Check the NAL unit type */
			guint8 nal_unit_type = map.data[i+2] == 0x01 ? map.data[i+3] & 0x1F : map.data[i+4] & 0x1F;
			if (nal_unit_type == 0x07) {
				//std::cout << "Buffer contains SPS" << std::endl;
				g_print("Buffer contains SPS at byte %zu\n", i);
			} else if (nal_unit_type == 0x08) {
				//std::cout << "Buffer contains PPS" << std::endl;
				g_print("Buffer contains PPS at byte %zu\n", i);
			}
		}
	}

	/* Unmap the buffer */
	gst_buffer_unmap(buffer, &map);

#endif

	GstFlowReturn ret;

	//TODO: Find a way to prevent copy , using gst_buffer_make_writable in here is not appropriate (see H15Encoding.cpp) encoder_output_sample
	//		which relates to sample buffer and changing the buffer directly will cause critical warnings.
	GstBuffer* EncBuffer = gst_buffer_copy (buffer);

	SetGstBufferTime(EncBuffer);
	
	//std::cout << "pts = " << GST_BUFFER_PTS (EncBuffer) << std::endl;
	//std::cout << "dts = " << GST_BUFFER_DTS (EncBuffer) << std::endl;
	//std::cout << "duration = " << GST_BUFFER_DURATION (EncBuffer) << std::endl;

	g_signal_emit_by_name (GST_APP_SRC(m_gstPipelineData.srcStream), "push-buffer", EncBuffer, &ret);
	gst_buffer_unref(EncBuffer); 

	return returnCode;
}


eStreamRetCode H15Streaming::feedData(char* buffer, uint32_t size)
{
	eStreamRetCode returnCode = STREAM_SUCCESS;

#if 0
	static int feedindex = 0;

	/* Iterate over the buffer data */
	for (gsize i = 0; i < size - 4; i++) {
		/* Look for the start code (0x000001 or 0x00000001) */
		if ((buffer[i] == 0x00 && buffer[i+1] == 0x00 && buffer[i+2] == 0x01) ||
			(buffer[i] == 0x00 && buffer[i+1] == 0x00 && buffer[i+2] == 0x00 && buffer[i+3] == 0x01)) {
			/* Check the NAL unit type */
			guint8 nal_unit_type = buffer[i+2] == 0x01 ? buffer[i+3] & 0x1F : buffer[i+4] & 0x1F;
			if (nal_unit_type == 0x07) {
				std::cout << "Buffer contains SPS at feed index " << feedindex << std::endl;
				g_print("Buffer contains SPS at byte %zu\n", i);
			} else if (nal_unit_type == 0x08) {
				std::cout << "Buffer contains PPS at feed index"  << feedindex << std::endl;
				g_print("Buffer contains PPS at byte %zu\n", i);
			}
		}
	}
	feedindex++;

#endif
	
	if (m_state != eStreamingState::STREAMING_STATE_RUNNING) {
		return eStreamRetCode::STREAM_NOT_RUNNING;
	}
	
	//Just don't send anything if there is no client(s) connected or pipeline is not ready (because of client not yet connected)
	if (m_numClientConnected <= 0 || m_gstPipelineData.srcStream == nullptr) {
		return eStreamRetCode::STREAM_SUCCESS;
	}

    GstBuffer *gstbuffer;
    GstMapInfo map;
	GstFlowReturn ret;	

    /* 	Create a new empty gstbuffer and fill the content 
		TODO: Find a way to prevent copy , this is related to application (eg media library's buffer).
	*/
    gstbuffer = gst_buffer_new_and_alloc(size);

    if (gst_buffer_map(gstbuffer, &map, GST_MAP_WRITE)) {
        memcpy(map.data, buffer, size);
        gst_buffer_unmap(gstbuffer, &map);
    }

	SetGstBufferTime(gstbuffer);

	g_signal_emit_by_name (GST_APP_SRC(m_gstPipelineData.srcStream), "push-buffer", gstbuffer, &ret);

    gst_buffer_unref(gstbuffer);	

	return returnCode;
}

H15EncoderConfig H15Streaming::GetEncodingInfo(std::string &jsonEncodingInfo)
{
	H15EncoderConfig config;

	nlohmann::json j = nlohmann::json::parse(jsonEncodingInfo);

	config.VideoFormat = VIDEO_FORMAT_NV12; //Only NV12 is supported

	if (j["encoding"]["hailo_encoder"]["config"]["output_stream"]["codec"] == "CODEC_TYPE_H264")
		config.Encoding = H264;
	else
		config.Encoding = H265;

	config.width = j["encoding"]["input_stream"]["width"];
	config.height = j["encoding"]["input_stream"]["height"];
	config.desiredFPS = j["encoding"]["input_stream"]["framerate"];
	config.bitrate = j["encoding"]["hailo_encoder"]["rate_control"]["bitrate"]["target_bitrate"];

	return config;
}


eStreamRetCode H15Streaming::SetGstBufferTime(GstBuffer* gstbuffer)
{
	eStreamRetCode returnCode = STREAM_SUCCESS;

	GST_BUFFER_PTS (gstbuffer) = m_gstPipelineData.m_Pts;
	GST_BUFFER_DTS (gstbuffer) = m_gstPipelineData.m_Pts;
	
	static auto PreviousTime = std::chrono::steady_clock::now();
	if (m_gstPipelineData.m_Pts == 0 ) {
		PreviousTime = std::chrono::steady_clock::now();
		GST_BUFFER_DURATION (gstbuffer) = gst_util_uint64_scale_int (1, 
																	 GST_SECOND, 
																	 m_gstPipelineData.streamingConfig.encodingConfig.desiredFPS);
	}
	else {
		auto CurrentTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(CurrentTime - PreviousTime).count();
		PreviousTime = CurrentTime;
		GST_BUFFER_DURATION(gstbuffer) = duration;
	}
	

	m_gstPipelineData.m_Pts = m_gstPipelineData.m_Pts + GST_BUFFER_DURATION (gstbuffer);

	return returnCode;

}
