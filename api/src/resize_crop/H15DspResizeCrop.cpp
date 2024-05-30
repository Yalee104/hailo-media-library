/**
 * Copyright (c) 2020-2023 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/

/* 
 * File:   H15ResizeCrop.cpp
 * Author: Aaron Lee
 * 
 * Created on Sept 8, 2023, 11:32 AM
 */

#include "H15DspResizeCrop.hpp"
#include <cassert>

dsp_image_format_t TranslateToDspFormat(eResizeCropBufferFormat format)
{
	if (format == DSP_NV12)
	{
		return DSP_IMAGE_FORMAT_NV12;
	}

	return DSP_IMAGE_FORMAT_RGB;
}


H15DspResizeCrop::H15DspResizeCrop(uint width, uint height, eResizeCropBufferFormat format, size_t max_buffers) :  
																					  m_width((width < 2) ? 2 : width),
																					  m_height((height < 2) ? 2 : height),
																					  m_format(format),
																					  m_max_buffers((max_buffers <= 1) ? 2 : max_buffers)
{

    dsp_status dsp_ret = dsp_utils::acquire_device();
    if (dsp_ret != DSP_SUCCESS)
    {
		std::cout << __FUNCTION__ << " FATAL: DSP device init failed" << std::endl;
        m_state = DSP_DEVICE_INIT_FAILED;
        return;
    }

	m_buffer_pool = std::make_shared<MediaLibraryBufferPool>(	m_width, 
																m_height, 
																TranslateToDspFormat(m_format), 
																m_max_buffers, CMA);
	if (m_buffer_pool->init() != MEDIA_LIBRARY_SUCCESS)
	{
		std::cout << __FUNCTION__ << " FATAL: Failed to init buffer pool: " << std::endl;
		m_state = DSP_DEVICE_INIT_FAILED;
	}
	else {
		m_state = DSP_RESIZE_CROP_INITIALIZED;
	}
}

H15DspResizeCrop::~H15DspResizeCrop() 
{
	dsp_status status = dsp_utils::release_device();
    if (status != DSP_SUCCESS)
    {
		std::cout << __FUNCTION__ << " WARNING: DSP device failed to release" << std::endl;
    }
}

HailoMediaLibraryBufferPtr H15DspResizeCrop::Resize(  HailoMediaLibraryBufferPtr framesource, 
									dsp_interpolation_type_t interpolation, 
									bool keepAspectRatio /* = false */)
{

	//We only want resize, so set cropTarget to 0
	dsp_utils::crop_resize_dims_t cropTarget;
	cropTarget.perform_crop = 0;

	return ProcessCropAndResize( framesource, cropTarget, interpolation, keepAspectRatio);
}

HailoMediaLibraryBufferPtr H15DspResizeCrop::CropAndResize(   HailoMediaLibraryBufferPtr framesource, 
											DspCropRect cropTarget, 
											dsp_interpolation_type_t interpolation,
											bool keepAspectRatio /* = false */)
{
	//We do not allow crop and resize with keep aspect ratio for now as we use caching for the buffer and cropping 
	//will change the actual size dynamically which will result in new cache key all the time and we will not be able to reuse the buffer
	//efficiently

	dsp_utils::crop_resize_dims_t cropParams;
	cropParams.perform_crop = 1;
	cropParams.crop_start_x = ROUND_TO_EVEN(cropTarget.start_x);
	cropParams.crop_start_y = ROUND_TO_EVEN(cropTarget.start_y);
	cropParams.crop_end_x = ROUND_TO_EVEN(cropTarget.end_x);
	cropParams.crop_end_y = ROUND_TO_EVEN(cropTarget.end_y);

	return ProcessCropAndResize( framesource, cropParams, interpolation, keepAspectRatio);
}


DspImageSize calculateResizeSize(DspImageSize &OriginalSize, DspImageSize &TargetSize, bool keepAspectRatio) 
{
    
	if (!keepAspectRatio) {
		return TargetSize;
	}

	DspImageSize trueSize;
    double originalAspectRatio = static_cast<double>(OriginalSize.frame_width) / OriginalSize.frame_height;
    double targetAspectRatio = static_cast<double>(TargetSize.frame_width) / TargetSize.frame_height;

    if (originalAspectRatio > targetAspectRatio) {
        // Original image is wider than target size. Width will be equal to target width.
		trueSize.frame_width = ROUND_TO_EVEN(TargetSize.frame_width);
		trueSize.frame_height = ROUND_TO_EVEN(static_cast<int>(TargetSize.frame_width / originalAspectRatio));
    } else {
        // Original image is taller than target size. Height will be equal to target height.
		trueSize.frame_width = ROUND_TO_EVEN(static_cast<int>(TargetSize.frame_height * originalAspectRatio));
		trueSize.frame_height = ROUND_TO_EVEN(TargetSize.frame_height);
    }

	return trueSize;
}


HailoMediaLibraryBufferPtr H15DspResizeCrop::ProcessCropAndResize( 	HailoMediaLibraryBufferPtr framesource, 
																	dsp_utils::crop_resize_dims_t cropTarget, 
																	dsp_interpolation_type_t interpolation, 
																	bool keepAspectRatio)
{

	hailo_media_library_buffer hailo_out_buffer;
    if (m_buffer_pool->acquire_buffer(hailo_out_buffer) != MEDIA_LIBRARY_SUCCESS)
	{
		std::cout << __FUNCTION__ << " FATAL: Failed to acquire buffer: " << std::endl;
		m_state = DSP_BUFFER_ALLOC_FAILED;
		return NULL;
	}

	assert(framesource);
	assert(framesource->hailo_pix_buffer);

	dsp_status ret;

	if (keepAspectRatio) {

		//To use the dsp letterbox we will need to provide crop (at least for the release version 1.3.0)
		//Therefore we simply set the crop to crop the entire source image if crop is not required from application level
		if (cropTarget.perform_crop == 0) {
			cropTarget.perform_crop = 1;
			cropTarget.crop_start_x = 0;
			cropTarget.crop_start_y = 0;
			cropTarget.crop_end_x = ROUND_DOWN_TO_EVEN(framesource->hailo_pix_buffer->width);
			cropTarget.crop_end_y = ROUND_DOWN_TO_EVEN(framesource->hailo_pix_buffer->height);
		}

        //Fixed alignment and color for now as we want to keep API backward compatibility
		//If needed to be exposed we will think of a way to maintain compatibility.
        dsp_letterbox_properties_t letterbox_params = {
            .alignment = DSP_LETTERBOX_MIDDLE, //DSP_LETTERBOX_UP_LEFT,
            .color = {.y = 0, .u = 128, .v = 128 },
        };

	  	ret = dsp_utils::perform_crop_and_resize_letterbox(	framesource->hailo_pix_buffer.get(),
															hailo_out_buffer.hailo_pix_buffer.get(),         	                		
                			          						cropTarget,
                          									interpolation,
															letterbox_params);
	}
	else {
	  	ret = dsp_utils::perform_crop_and_resize(	framesource->hailo_pix_buffer.get(),
													hailo_out_buffer.hailo_pix_buffer.get(),        	                		
        	                  						cropTarget,
            	              						interpolation);
	}

    if (ret != DSP_SUCCESS) {
		hailo_out_buffer.decrease_ref_count();
		std::cout << "Failed to resize, code: " << ret << std::endl;
		m_state = DSP_RESIZE_FAILED;
		return NULL;
	}

	HailoMediaLibraryBufferPtr hailo_buffer_ptr = std::make_shared<hailo_media_library_buffer>(std::move(hailo_out_buffer));
	
	return hailo_buffer_ptr;

}

