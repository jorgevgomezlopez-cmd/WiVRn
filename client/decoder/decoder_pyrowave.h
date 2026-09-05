#pragma once

#include "decoder.h"
#include "wivrn_packets.h"

#include <iostream>
#include <span>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

namespace scenes {
    class stream;
}

namespace wivrn {

    class decoder_pyrowave : public decoder {
    private:
        // Handle RAII de Vulkan para mantener vivo el sampler mientras exista la clase
        vk::raii::Sampler m_sampler{nullptr};

    public:
        decoder_pyrowave(
            vk::raii::Device & device,
            vk::raii::PhysicalDevice & phys_dev,
            uint32_t vk_queue_family_index,
            const wivrn::to_headset::video_stream_description & description,
            uint8_t stream_index,
            std::weak_ptr<scenes::stream> scene,
            shard_accumulator * acc);

        ~decoder_pyrowave() override = default;

        // Métodos heredados de la interfaz 'decoder'
        void push_data(std::span<std::span<const uint8_t>> data, uint64_t frame_index, bool partial) override;

        void frame_completed(
            const from_headset::feedback & feedback,
            const to_headset::video_stream_data_shard::view_info_t & view_info) override;

            vk::Sampler sampler() override;

            // Método propio para procesar cada paquete de red
            void push_shard(uint64_t frame_index, std::span<const uint8_t> data);
    };

} // namespace wivrn
