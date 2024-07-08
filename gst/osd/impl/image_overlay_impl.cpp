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

#include "image_overlay_impl.hpp"
#include <opencv2/core/utils/filesystem.hpp>
#include "media_library/media_library_logger.hpp"

ImageOverlayImpl::ImageOverlayImpl(const osd::ImageOverlay &overlay, media_library_return &status) : OverlayImpl(overlay.id, overlay.x, overlay.y, overlay.width, overlay.height, overlay.z_index, overlay.angle, overlay.rotation_alignment_policy, false),
                                                                                                     m_path(overlay.image_path)
{
    status = MEDIA_LIBRARY_SUCCESS;
}

tl::expected<ImageOverlayImplPtr, media_library_return> ImageOverlayImpl::create(const osd::ImageOverlay &overlay)
{
    media_library_return status = MEDIA_LIBRARY_UNINITIALIZED;
    auto osd_overlay = std::make_shared<ImageOverlayImpl>(overlay, status);
    if (status != MEDIA_LIBRARY_SUCCESS)
    {
        return tl::make_unexpected(status);
    }
    return osd_overlay;
}

std::shared_future<tl::expected<ImageOverlayImplPtr, media_library_return>> ImageOverlayImpl::create_async(const osd::ImageOverlay &overlay)
{
    return std::async(std::launch::async, [overlay]()
                      { return create(overlay); })
        .share();
}

tl::expected<std::vector<dsp_overlay_properties_t>, media_library_return> ImageOverlayImpl::create_dsp_overlays(int frame_width, int frame_height)
{
    if (frame_width == 0 || frame_height == 0)
    {
        return tl::make_unexpected(MEDIA_LIBRARY_UNINITIALIZED);
    }

    // check if the file exists
    if (!cv::utils::fs::exists(m_path))
    {
        LOGGER__ERROR("Error: file {} does not exist", m_path);
        return tl::make_unexpected(MEDIA_LIBRARY_INVALID_ARGUMENT);
    }

    // read the image from file, keeping alpha channel
    m_image_mat = cv::imread(m_path, cv::IMREAD_UNCHANGED);

    // check if the image was read successfully
    if (m_image_mat.empty())
    {
        LOGGER__ERROR("Error: failed to read image file {}", m_path);
        return tl::make_unexpected(MEDIA_LIBRARY_INVALID_ARGUMENT);
    }

    // convert image to 4-channel RGBA format if necessary
    if (m_image_mat.channels() != 4)
    {
        LOGGER__INFO("READ IMAGE THAT WAS NOT 4 channels");
        cv::cvtColor(m_image_mat, m_image_mat, cv::COLOR_BGR2BGRA);
    }

    m_image_mat = resize_mat(m_image_mat, m_width * frame_width, m_height * frame_height);

    return OverlayImpl::create_dsp_overlays(frame_width, frame_height);
}

std::shared_ptr<osd::Overlay> ImageOverlayImpl::get_metadata()
{
    return std::make_shared<osd::ImageOverlay>(m_id, m_x, m_y, m_width, m_height, m_path, m_z_index, m_angle, m_rotation_policy);
}
