#pragma once

#include "video_encoder.h"
#include <vulkan/vulkan_raii.hpp>

namespace wivrn
{

    struct vk_bundle;

    class video_encoder_vulkan_pyrowave : public video_encoder
    {
        vk_bundle & vk;

        uint32_t stream_width{0};
        uint32_t stream_height{0};

        // Recursos de Vulkan
        vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
        vk::raii::PipelineLayout pipeline_layout{nullptr};
        vk::raii::Pipeline compute_pipeline{nullptr};

        vk::raii::DescriptorPool descriptor_pool{nullptr};
        vk::raii::DescriptorSets descriptor_sets{nullptr};
        vk::raii::CommandPool command_pool{nullptr};

        // Buffer de salida (SSBO) accesible desde CPU
        vk::raii::Buffer output_buffer{nullptr};
        vk::raii::DeviceMemory output_memory{nullptr};
        size_t output_buffer_size{0};

        void init_compute_pipeline();
        void create_output_buffer(size_t size);

    public:
        // Traer sobrecargas de la clase base para evitar que C++ las oculte (-Woverloaded-virtual)
        using video_encoder::present_image;

        // Métodos de la clase base
        void reset() override;

        video_encoder_vulkan_pyrowave(
            vk_bundle & vk,
            const encoder_settings & settings,
            uint8_t stream_idx);

        // Firma de 3 parámetros (invocada directamente por layer_commit)
        void present_image(
            vk::Image y_cbcr,
            vk::SemaphoreSubmitInfo sem_info,
            uint64_t frame_index) ;

            // Sobrecarga de 4 parámetros (para compatibilidad de interfaz interna)
            void present_image(
                vk::Image y_cbcr,
                vk::SemaphoreSubmitInfo sem_info,
                uint8_t slot,
                uint64_t frame_index) override;

            std::optional<data> encode(
                uint8_t slot,
                uint64_t frame_index) override;
    };

} // namespace wivrn
