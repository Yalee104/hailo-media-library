#pragma once

#include "stages.hpp"
#include "hailo/hailort.hpp"

class HailortAsyncStage : public ProducableStage<BufferPtr, BufferPtr>
{
private:
    std::unordered_map<std::string, MediaLibraryBufferPoolPtr> m_output_buffer_pool;

    std::unique_ptr<hailort::VDevice> m_vdevice;
    std::shared_ptr<hailort::InferModel> m_infer_model;
    hailort::ConfiguredInferModel m_configured_infer_model;
    hailort::ConfiguredInferModel::Bindings m_bindings;
    std::string m_name;
    int m_output_pool_size;
    std::string m_hef_path;
    std::string m_group_id;
    int m_batch_size;
    std::queue<BufferPtr> m_batch_queue;
    
public:
    HailortAsyncStage(std::string name, size_t queue_size, int output_pool_size, std::string hef_path, std::string group_id, int batch_size) :
        ProducableStage(name, queue_size, drop_buffer), m_name(name), m_output_pool_size(output_pool_size), m_hef_path(hef_path), m_group_id(group_id), m_batch_size(batch_size) { }

    int init() override
    {
        hailo_vdevice_params_t vdevice_params = {0};
        hailo_init_vdevice_params(&vdevice_params);
        vdevice_params.group_id = m_group_id.c_str();        
        //std::cout << "Hailo Device Count = " << vdevice_params.device_count << ", Group Id: " << vdevice_params.group_id << std::endl;

        auto vdevice_exp = hailort::VDevice::create(vdevice_params);

        if (!vdevice_exp) {
            std::cerr << "Failed create vdevice, status = " << vdevice_exp.status() << std::endl;
            return vdevice_exp.status();
        }
        m_vdevice = vdevice_exp.release();

        auto infer_model_exp = m_vdevice->create_infer_model(m_hef_path.c_str());
        if (!infer_model_exp) {
            std::cerr << "Failed to create infer model, status = " << infer_model_exp.status() << std::endl;
            return infer_model_exp.status();
        }
        m_infer_model = infer_model_exp.release();
        m_infer_model->set_batch_size(m_batch_size);

        //m_infer_model->input()->set_format_type(HAILO_FORMAT_TYPE_UINT8);

        auto configured_infer_model_exp = m_infer_model->configure();
        if (!configured_infer_model_exp) {
            std::cerr << "Failed to create configured infer model, status = " << configured_infer_model_exp.status() << std::endl;
            return configured_infer_model_exp.status();
        }
        m_configured_infer_model = configured_infer_model_exp.release();
        m_configured_infer_model.set_scheduler_threshold(4);
        m_configured_infer_model.set_scheduler_timeout(std::chrono::milliseconds(100));

        auto bindings = m_configured_infer_model.create_bindings();
        if (!bindings) {
            std::cerr << "Failed to create infer bindings, status = " << bindings.status() << std::endl;
            return bindings.status();
        }
        m_bindings = bindings.release();

        for (const auto &output_name : m_infer_model->get_output_names()) {
            size_t output_frame_size = m_infer_model->output(output_name)->get_frame_size();
            std::cout << "output name: " << output_name << ", output_frame_size: " << output_frame_size << std::endl;
            m_output_buffer_pool[output_name] = std::make_shared<MediaLibraryBufferPool>(   output_frame_size, 1, DSP_IMAGE_FORMAT_GRAY8,
                                                                                            m_output_pool_size, CMA, output_frame_size);
            
            if (m_output_buffer_pool[output_name]->init() != MEDIA_LIBRARY_SUCCESS)
            {
                return ERROR;
            }

#if 0
            std::vector<hailo_quant_info_t> quantization_info = m_infer_model->output(output_name)->get_quant_infos();
            std::cout << "quantization_info size: " << quantization_info.size() << std::endl;
            std::cout << "quantization_info qp_zp: " << quantization_info[0].qp_zp << std::endl;
            std::cout << "quantization_info qp_scale: " << quantization_info[0].qp_scale << std::endl;
            std::cout << "quantization_info limvals_min: " << quantization_info[0].limvals_min << std::endl;
            std::cout << "quantization_info limvals_max: " << quantization_info[0].limvals_max << std::endl;
#endif
        }
       
        return SUCCESS;
    }

    int set_pix_buf(const HailoMediaLibraryBufferPtr buffer)
    {
        auto y_plane_buffer = buffer->get_plane(0);
        uint32_t y_plane_size = buffer->get_plane_size(0);

        auto uv_plane_buffer = buffer->get_plane(1);
        uint32_t uv_plane_size = buffer->get_plane_size(1);

        //std::cout << "y_plane_size: " << y_plane_size << std::endl;
        //std::cout << "uv_plane_size: " << uv_plane_size << std::endl;

        hailo_pix_buffer_t pix_buffer{};
        pix_buffer.memory_type = HAILO_PIX_BUFFER_MEMORY_TYPE_USERPTR;
        pix_buffer.number_of_planes = 2;
        pix_buffer.planes[0].bytes_used = y_plane_size;
        pix_buffer.planes[0].plane_size = y_plane_size; 
        pix_buffer.planes[0].user_ptr = reinterpret_cast<void*>(y_plane_buffer);

        pix_buffer.planes[1].bytes_used = uv_plane_size;
        pix_buffer.planes[1].plane_size = uv_plane_size;
        pix_buffer.planes[1].user_ptr = reinterpret_cast<void*>(uv_plane_buffer);

        auto status = m_bindings.input()->set_pix_buffer(pix_buffer);
        if (HAILO_SUCCESS != status) {
            std::cerr << "Failed to set infer input buffer, status = " << status << std::endl;
            return status;
        }

        return SUCCESS;
    }


    int prepare_output(std::unordered_map<std::string, BufferPtr> &output_buffers_ptr_list, BufferPtr input_buffer)
    {

        for (const auto &output_name : m_infer_model->get_output_names()) {
            size_t output_frame_size = m_infer_model->output(output_name)->get_frame_size();
            std::vector<hailo_quant_info_t> quantization_info = m_infer_model->output(output_name)->get_quant_infos();


            BufferPtr output_buffer_ptr = std::make_shared<Buffer>(false);
            output_buffer_ptr->add_media_lib_buffer(MediaLibraryBufferType::Hailort, HailoMediaLibraryBufferPtr(new hailo_media_library_buffer));
            output_buffer_ptr->copy_media_lib_buffers(input_buffer);

            output_buffer_ptr->copy_metadata(input_buffer);
            output_buffer_ptr->set_media_lib_buffer_id(MediaLibraryBufferType::Hailort, output_name);

            QuantInfoBufferMetadataPtr quant_info_metadata = std::make_shared<QuantInfoBufferMetadata>(quantization_info);
            output_buffer_ptr->append_metadata(quant_info_metadata);

            HailoMediaLibraryBufferPtr output_buffer = output_buffer_ptr->media_lib_buffers_list[MediaLibraryBufferType::Hailort];
            if (m_output_buffer_pool[output_name]->acquire_buffer(*output_buffer) != MEDIA_LIBRARY_SUCCESS)
            {
                std::cerr << "Failed to acquire buffer" << std::endl;
                return ERROR;
            }

            auto status = m_bindings.output(output_name)->set_buffer(hailort::MemoryView(output_buffer->get_plane(0), output_frame_size));
            if (HAILO_SUCCESS != status) {
                std::cerr << "Failed to set infer output buffer, status = " << status << std::endl;
                return status;
            }

            output_buffers_ptr_list[output_name] = output_buffer_ptr;
        }

        return SUCCESS;
    }


    int infer(HailoMediaLibraryBufferPtr input_buffer, std::unordered_map<std::string, BufferPtr> output_buffers_ptr_list)
    {
        auto status = m_configured_infer_model.wait_for_async_ready(std::chrono::milliseconds(1000));
        if (HAILO_SUCCESS != status) {
            std::cerr << "Failed to wait for async ready, status = " << status << std::endl;
            return status;
        }

        auto job = m_configured_infer_model.run_async(m_bindings, [output_buffers_ptr_list, input_buffer, this](const hailort::AsyncInferCompletionInfo& completion_info) {
            if (completion_info.status != HAILO_SUCCESS) {
                std::cerr << "Failed to run async infer, status = " << completion_info.status << std::endl;
                return ERROR;
            }

            for (const auto& output_buffer : output_buffers_ptr_list)
            {
                send_to_subscribers(output_buffer.second);
            }

            return SUCCESS;
        });

        if (!job) {
            std::cerr << "Failed to start async infer job, status = " << job.status() << std::endl;
            return job.status();
        }

        job->detach();

        return SUCCESS;
    }

    int process(BufferPtr data)
    {
        m_batch_queue.push(data);

        if (static_cast<int>(m_batch_queue.size()) < m_batch_size)
        {
            return SUCCESS;
        }

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        for (int i=0; i < m_batch_size; i++)
        {
            auto input_buffer = m_batch_queue.front();
            m_batch_queue.pop();
            
            if (set_pix_buf(input_buffer->media_lib_buffers_list[MediaLibraryBufferType::Cropped]) != SUCCESS)
            {
                return ERROR;
            }

            std::unordered_map<std::string, BufferPtr> output_buffers_ptr_list;
            if (prepare_output(output_buffers_ptr_list, input_buffer) != SUCCESS)
            {
                return ERROR;
            }


            if (infer(input_buffer->media_lib_buffers_list[MediaLibraryBufferType::Cropped], output_buffers_ptr_list) != SUCCESS)
            {
                return ERROR;
            }
        }

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

        if (PRINT_STATS)
        {
            std::cout << "AI time = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[micro]" << std::endl;
        }

        return SUCCESS;
    }
};