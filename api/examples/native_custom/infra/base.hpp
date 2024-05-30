#pragma once

#include "buffer_utils.hpp"
#include "Utils/hailo-common/hailo_common.hpp"

#include <thread>
#include <vector>
#include <unordered_map>

#define PRINT_STATS 0
#define SUCCESS 1
#define ERROR -1

struct stletterbox {
    bool is_performed = false;
    size_t  offset_x = 0;
    size_t  offset_y = 0;
    size_t  width = 0;
    size_t  height = 0;
};

struct BBox {
    float confidence;
    int detection_class;
    size_t x;
    size_t y;
    size_t width;
    size_t height;
};

enum class BufferMetadataType
{
    BBox,
    Cropped,
    QuantizationInfo
};

enum class MediaLibraryBufferType
{
    Unknown,
    HD720p,
    Stream4K,
    Cropped,
    Hailort
};


class BufferMetadata 
{
public:
    BufferMetadataType type;

    BufferMetadata(BufferMetadataType type) : type(type) {}

    BufferMetadataType get_type()
    {
        return type;
    }
};
using BufferMetadataPtr = std::shared_ptr<BufferMetadata>;


class Buffer
{
private:
    std::unordered_map<BufferMetadataType, BufferMetadataPtr> metadata_list;

public:
    std::unordered_map<MediaLibraryBufferType, HailoMediaLibraryBufferPtr> media_lib_buffers_list;
    std::unordered_map<MediaLibraryBufferType, std::string> media_lib_buffers_list_id;

    bool created_with_deleter = false;

    Buffer(bool created_with_deleter) : media_lib_buffers_list(), created_with_deleter(created_with_deleter) {}

    void set_media_lib_buffer_id(const MediaLibraryBufferType key, std::string id)
    {
        media_lib_buffers_list_id[key] = id;
    }

    std::string get_media_lib_buffer_id(const MediaLibraryBufferType key)
    {
        return media_lib_buffers_list_id[key];
    }

    void add_media_lib_buffer(const MediaLibraryBufferType key, HailoMediaLibraryBufferPtr buffer)
    {
        media_lib_buffers_list[key] = buffer;
    }

    void copy_media_lib_buffers(std::shared_ptr<Buffer> source_buffer)
    {
        for (const auto& buffer : source_buffer->media_lib_buffers_list)
        {
            media_lib_buffers_list[buffer.first] = buffer.second;

            media_lib_buffers_list[buffer.first]->increase_ref_count();
        }

    }

    bool has_key(const BufferMetadataType& key)
    {
        return metadata_list.find(key) != metadata_list.end();
    }

    bool has_Bufferkey(const MediaLibraryBufferType& key)
    {
        return media_lib_buffers_list.find(key) != media_lib_buffers_list.end();
    }

    void copy_metadata(std::shared_ptr<Buffer> buffer)
    {
        for (auto &metadata : buffer->metadata_list)
        {
            metadata_list[metadata.first] = metadata.second;
        }
    }

    void append_metadata(BufferMetadataPtr metadata)
    {
        metadata_list[metadata->get_type()] = metadata;
    }

    void remove_metadata(const BufferMetadataType& key)
    {
        metadata_list.erase(key);
    }

    BufferMetadataPtr get_metadata(const BufferMetadataType& key)
    {
        auto it = metadata_list.find(key);
        return it != metadata_list.end() ? it->second : nullptr; 
    }

    void increase_refcounts()
    {
        if (!created_with_deleter)
        {
            for (auto& [key, media_lib_buffer] : media_lib_buffers_list)
            {
                media_lib_buffer->increase_ref_count();
            }
        }
    }

    void decrease_refcounts()
    {
        if (!created_with_deleter)
        {
            for (auto& [key, media_lib_buffer] : media_lib_buffers_list)
            {
                media_lib_buffer->decrease_ref_count();
            }
        }
    }

    ~Buffer() 
    {
        if (!created_with_deleter)
        {
            for (auto& [key, media_lib_buffer] : media_lib_buffers_list)
            {
                media_lib_buffer->decrease_ref_count();
            }
        }
    }
};
using BufferPtr = std::shared_ptr<Buffer>;
