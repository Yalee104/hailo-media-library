/*
 * Copyright (c) 2017-2024 Hailo Technologies Ltd. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#pragma once
#include <iostream>
#include <memory>
#include <unordered_map>

extern "C"
{
#include "video_encoder/base_type.h"
#include "video_encoder/encinputlinebuffer.h"
#include "video_encoder/ewl.h"
#include "video_encoder/hevcencapi.h"
}

// Hailo includes
#include "buffer_pool.hpp"
#include "encoder_class.hpp"
#include "encoder_gop_config.hpp"
#include "encoder_internal.hpp"

enum encoder_stream_restart_t
{
  STREAM_RESTART_NONE = 0,
  STREAM_RESTART,
  STREAM_RESTART_HARD
};

enum encoder_config_type_t
{
  ENCODER_CONFIG_RATE_CONTROL = 0,
  ENCODER_CONFIG_PRE_PROCESSING,
  ENCODER_CONFIG_CODING_CONTROL,
  ENCODER_CONFIG_GOP,
  ENCODER_CONFIG_STREAM
};
struct EncoderCounters
{
  i32 picture_cnt;
  i32 picture_enc_cnt;
  u32 idr_interval;
  i32 last_idr_picture_cnt;
  u32 validencodedframenumber;
};

enum encoder_state_t
{
  ENCODER_STATE_UNINITIALIZED = 0,
  ENCODER_STATE_INITIALIZED,
  ENCODER_STATE_START,
  ENCODER_STATE_STOP
};

class Encoder::Impl final
{
private:
  const std::unordered_map<std::string, VCEncLevel> h265_level = {
      {"1.0", VCENC_HEVC_LEVEL_1}, {"2.0", VCENC_HEVC_LEVEL_2}, {"2.1", VCENC_HEVC_LEVEL_2_1}, {"3.0", VCENC_HEVC_LEVEL_3}, {"3.1", VCENC_HEVC_LEVEL_3_1}, {"4.0", VCENC_HEVC_LEVEL_4}, {"4.1", VCENC_HEVC_LEVEL_4_1}, {"5.0", VCENC_HEVC_LEVEL_5}, {"5.1", VCENC_HEVC_LEVEL_5_1}};
  const std::unordered_map<std::string, VCEncLevel> h264_level = {
      {"1.0", VCENC_H264_LEVEL_1}, {"1.1", VCENC_H264_LEVEL_1_1}, {"1.2", VCENC_H264_LEVEL_1_2}, {"1.3", VCENC_H264_LEVEL_1_3}, {"2.0", VCENC_H264_LEVEL_2}, {"2.1", VCENC_H264_LEVEL_2_1}, {"2.2", VCENC_H264_LEVEL_2_2}, {"3.0", VCENC_H264_LEVEL_3}, {"3.1", VCENC_H264_LEVEL_3_1}, {"3.2", VCENC_H264_LEVEL_3_2}, {"4.0", VCENC_H264_LEVEL_4}, {"4.1", VCENC_H264_LEVEL_4_1}, {"4.2", VCENC_H264_LEVEL_4_2}, {"5.0", VCENC_H264_LEVEL_5}, {"5.1", VCENC_H264_LEVEL_5_1}};
  const std::unordered_map<std::string, VCEncPictureType> input_formats = {
      {"I420", VCENC_YUV420_PLANAR},
      {"NV12", VCENC_YUV420_SEMIPLANAR},
      {"NV21", VCENC_YUV420_SEMIPLANAR_VU}};
  VCEncApiVersion m_encoder_version;
  VCEncBuild m_encoder_build;
  VCEncConfig m_vc_cfg;
  VCEncCodingCtrl m_vc_coding_cfg;
  VCEncRateCtrl m_vc_rate_cfg;
  VCEncPreProcessingCfg m_vc_pre_proc_cfg;
  uint32_t m_input_stride;

  // const char * const m_json_schema = load_json_schema();
  VCEncInst m_inst;
  VCEncIn m_enc_in;
  VCEncOut m_enc_out;
  int m_next_gop_size;
  VCEncPictureCodingType m_next_coding_type;
  EncoderCounters m_counters;
  void *m_ewl;
  bool m_multislice_encoding;
  EWLLinearMem_t m_output_memory;
  std::vector<HailoMediaLibraryBufferPtr> m_inputs;
  EncoderOutputBuffer m_header;
  std::shared_ptr<EncoderConfig> m_config;
  class gopConfig;
  std::unique_ptr<gopConfig> m_gop_cfg;
  MediaLibraryBufferPoolPtr m_buffer_pool;
  encoder_stream_restart_t m_stream_restart;
  encoder_state_t m_state;

  std::vector<encoder_config_type_t> m_update_required;

public:
  Impl(std::string json_string);
  ~Impl();
  std::vector<EncoderOutputBuffer> handle_frame(HailoMediaLibraryBufferPtr buf);
  void force_keyframe();
  void update_stride(uint32_t stride);
  int get_gop_size();
  media_library_return configure(std::string json_string);
  media_library_return configure(const encoder_config_t &config);
  encoder_config_t get_config();
  EncoderOutputBuffer start();
  EncoderOutputBuffer stop();
  media_library_return init();
  media_library_return release();
  media_library_return dispose();

  // static const char *json_schema const get_json_schema() const;
  // static const char * const load_json_schema() const;
private:
  void updateArea(coding_roi_t &area, VCEncPictureArea &vc_area);
  void updateArea(coding_roi_area_t &area, VCEncPictureArea &vc_area);
  int init_gop_config();
  void create_gop_config();
  void init_buffer_pool(uint pool_size);
  VCEncRet init_coding_control_config();
  VCEncRet init_rate_control_config();
  VCEncRet init_preprocessing_config();
  VCEncRet init_encoder_config();
  void stamp_time_and_log_fps(timespec &start_handle, timespec &end_handle);
  VCEncLevel get_level(std::string level, bool codecH264);
  VCEncPictureType get_input_format(std::string format);
  VCEncPictureCodingType find_next_pic();
  media_library_return update_input_buffer(HailoMediaLibraryBufferPtr buf);
  media_library_return create_output_buffer(EncoderOutputBuffer &output_buf);
  int allocate_output_memory();
  media_library_return update_configurations();
  media_library_return update_gop_configurations();
  media_library_return stream_restart();
  media_library_return encode_header();
  media_library_return
  encode_frame(HailoMediaLibraryBufferPtr buf,
               std::vector<EncoderOutputBuffer> &outputs);
  media_library_return
  encode_multiple_frames(std::vector<EncoderOutputBuffer> &outputs);
  uint32_t get_codec();
  bool hard_restart_required(const hailo_encoder_config_t &new_config, bool gop_update_required);
  bool gop_config_update_required(const hailo_encoder_config_t &new_config);
  VCEncProfile get_profile();
};

class Encoder::Impl::gopConfig
{
private:
  VCEncGopConfig *m_gop_cfg;
  VCEncGopPicConfig m_gop_pic_cfg[MAX_GOP_PIC_CONFIG_NUM];
  int m_gop_size;
  char *m_gop_cfg_name;
  uint8_t m_gop_cfg_offset[MAX_GOP_SIZE + 1];
  int m_b_frame_qp_delta;
  bool m_codec_h264;
  int ReadGopConfig(std::vector<GopPicConfig> &config, int gopSize);
  int ParseGopConfigLine(GopPicConfig &pic_cfg, int gopSize);

public:
  gopConfig(VCEncGopConfig *gopConfig, int gopSize, int bFrameQpDelta,
            bool codecH264);
  int init_config(VCEncGopConfig *gopConfig, int gop_size, int b_frame_qp_delta, bool codec_h264);
  int get_gop_size() const;
  ~gopConfig() = default;
  VCEncGopPicConfig *get_gop_pic_cfg() { return m_gop_pic_cfg; }
  const uint8_t *get_gop_cfg_offset() const { return m_gop_cfg_offset; }
};