/**
 * Copyright (c) 2020-2023 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/

/* 
 * File:   H15ResizeCrop.hpp
 * Author: Aaron Lee
 * 
 * Created on Sept 8, 2023, 11:32 AM
 */

#ifndef _H15DSP_RESIZE_CROP_H_
#define _H15DSP_RESIZE_CROP_H_

#include <time.h>
#include <ctype.h>
#include <cstring> 
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <unordered_map>

#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <hailo/hailodsp.h>
#include "media_library/buffer_pool.hpp"
#include "media_library/dsp_utils.hpp"

// Define a macro that rounds a number to the nearest even number
#define ROUND_TO_EVEN(n) ((n) % 2 == 0 ? (n) : (n) + 1)
#define ROUND_DOWN_TO_EVEN(n) ((n) % 2 == 0 ? (n) : (n) - 1)
#define IS_EVEN(n) ((n) % 2 == 0)

typedef struct DspImageSize {

    uint                 frame_width;
    uint                 frame_height;

} DspImageSize;

typedef struct {
    /** Offset of the left most pixel in the cropped image. valid range: [0, width-1] */
    int start_x;
    /** Offset of the up most pixel in the cropped image. valid range: [0, height-1] */
    int start_y;
    /** Offset of the right most pixel in the cropped image. valid range: [1, width] */
    int end_x;
    /** Offset of the bottom most pixel in the cropped image. valid range: [1, height] */
    int end_y;
} DspCropRect;


enum eResizeCropBufferFormat {
    DSP_NV12    = 0,
};

enum eDspResizeCropState {
    
    //General Code List
    DSP_RESIZE_CROP_UNINITIALIZED               = 0,
    DSP_RESIZE_CROP_INITIALIZED                 = 1,
    
    //Failure Code list
    DSP_DEVICE_INIT_FAILED          =-1,
    DSP_INVALID_PARAM               =-2,
    DSP_UNSUPPORTED_FRAME_FORMAT    =-3,
    DSP_DEVICE_BUFFER_FAILED        =-4,
    DSP_RESIZE_FAILED               =-5,
    DSP_BUFFER_ALLOC_FAILED         =-6,  
 
};

class H15DspResizeCrop;
using H15DspResizeCropPtr = std::shared_ptr<H15DspResizeCrop>;


class H15DspResizeCrop
{


private:
    eDspResizeCropState         m_state = DSP_RESIZE_CROP_UNINITIALIZED;
    uint                        m_width;
    uint                        m_height;
    eResizeCropBufferFormat     m_format = DSP_NV12;
    size_t                      m_max_buffers;
    MediaLibraryBufferPoolPtr   m_buffer_pool;

public:

    /**
     * Constructor and initialize
     * NOTE: This object will reserve the maximum number of buffers to be allocated based on the given width and height
     *       of the output image after calling resize or crop and resize APIs. 
     *       The output width and height cannot be changed after the object is created.
     *       It is suggested to create a new object for different sizes of output image and keep the object alive for maximum performance.
     * 
     * WARNING: For DSP_NV12 the width and height MUST be even number
     * 
     * @param width         The width of the image
     * @param height        The height of the image
     * @param format        The format of the image
     * @param max_buffers   The maximum number of buffers to be allocated
     * WARNING: The maximum number of buffers MUST be greater than 1
     * @return NONE
     */
    H15DspResizeCrop(uint width, uint height, eResizeCropBufferFormat format, size_t max_buffers);

    ~H15DspResizeCrop();

    /**
     * Resize the image
     * @param framesource       The source frame image buffer
     * @param interpolation     The interpolation type
     * @param keepAspectRatio   Keep the aspect ratio of the image and align image to the middle while the rest of pixel filled with black
     * @return HailoMediaLibraryBufferPtr the returned resized image buffer
     * WARNING: The returned resized image MUST be freed by calling HailoMediaLibraryBufferPtr->decrease_ref_count() to release the allocated memory
     */
    HailoMediaLibraryBufferPtr Resize(  HailoMediaLibraryBufferPtr framesource, 
                                        dsp_interpolation_type_t interpolation, 
                                        bool keepAspectRatio = false);


    /**
     * Crop first and resize the image
     * @param framesource       The source frame image buffer
     * @param cropTarget        The crop target
     * @param interpolation     The interpolation type
     * @param keepAspectRatio   Keep the aspect ratio of the image and align image to the middle while the rest of pixel filled with black
     * @return HailoMediaLibraryBufferPtr the returned resized image buffer
     * WARNING: The returned cropped image MUST be freed by calling H15FrameBufferBase::AutoDestroy() to release the allocated memory
     * NOTE:    For DSP_IMAGE_FORMAT_NV12 the cropTarget, resizeTarget width and height MUST be even number
     */
    HailoMediaLibraryBufferPtr CropAndResize(   HailoMediaLibraryBufferPtr framesource, 
                                                DspCropRect cropTarget, 
                                                dsp_interpolation_type_t interpolation,
                                                bool keepAspectRatio = false);

    /**
     * Get the current state in case of any failure from API calls
     * @return the state base on eDspResizeCropState
     */
    eDspResizeCropState GetCurrentState() { return m_state; };

    /**
     * Get the target resize width
     * @return the target resize width
     */
    uint GetWidth() { return m_width; };

    /**
     * Get the target resize height
     * @return the target resize height
     */
    uint GetHeight() { return m_height; };

private:

    HailoMediaLibraryBufferPtr ProcessCropAndResize( HailoMediaLibraryBufferPtr framesource, dsp_utils::crop_resize_dims_t cropTarget, 
                                                     dsp_interpolation_type_t interpolation, bool keepAspectRatio = false);

};



#endif /* _H15DSP_RESIZE_CROP_H_ */
