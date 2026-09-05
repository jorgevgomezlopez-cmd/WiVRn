#pragma once

#include "decoder.h"
#include <iostream>

namespace wivrn {

    class decoder_pyrowave : public decoder {
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

        // Métodos virtuales obligatorios heredados de la clase base 'decoder'
        void push_data(std::span<std::span<const uint8_t>> data, uint64_t frame_index, bool partial) override;
        void frame_completed(uint64_t frame_index, std::shared_ptr<video_frame> frame) override;
        vk::Sampler sampler() override;

        // Si push_shard es un método propio de pyrowave y no existe en la clase base 'decoder',
        // quita el 'override':
        void push_shard(uint64_t frame_index, std::span<const uint8_t> data);
    };

} // namespace wivrn
