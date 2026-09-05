#include "decoder_pyrowave.h"

namespace wivrn {

    decoder_pyrowave::decoder_pyrowave(
        vk::raii::Device & device,
        vk::raii::PhysicalDevice & phys_dev,
        uint32_t vk_queue_family_index,
        const wivrn::to_headset::video_stream_description & description,
        uint8_t stream_index,
        std::weak_ptr<scenes::stream> scene,
        shard_accumulator * acc)
    : sampler(nullptr) // Inicialización explícita del miembro RAII
    {
        vk::SamplerCreateInfo sampler_info{};
        sampler_info.magFilter = vk::Filter::eLinear;
        sampler_info.minFilter = vk::Filter::eLinear;
        sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

        // Creación usando la interfaz RAII
        sampler = device.createSampler(sampler_info);

        std::cout << "[Pyrowave Client] Instanciado decodificador Pyrowave exitosamente." << std::endl;
    }

    void decoder_pyrowave::push_data(std::span<std::span<const uint8_t>> data, uint64_t frame_index, bool partial)
    {
        for (auto shard : data) {
            if (!shard.empty()) {
                push_shard(frame_index, shard);
            }
        }
    }

    void decoder_pyrowave::frame_completed(
        const from_headset::feedback & feedback,
        const to_headset::video_stream_data_shard::view_info_t & view_info)
    {
        // Puntos de entrada para notificar el frame a OpenXR cuando tengas el shader listo
    }

    vk::Sampler decoder_pyrowave::sampler()
    {
        // Devuelve el handle nativo de Vulkan a partir del objeto RAII
        return *sampler;
    }

    void decoder_pyrowave::push_shard(uint64_t frame_index, std::span<const uint8_t> data)
    {
        if (data.empty()) return;

        static uint64_t counter = 0;
        if (++counter % 90 == 0) {
            std::cout << "[Pyrowave Client] Frame #" << frame_index
            << " recibido en APK | Bytes: " << data.size()
            << " | Muestra: " << static_cast<int>(data[0]) << " " << static_cast<int>(data[1])
            << " " << static_cast<int>(data[2]) << " " << static_cast<int>(data[3]) << std::endl;
        }

        // TODO: Copiar 'data' al SSBO de Vulkan y ejecutar cmd.dispatch() del Compute Shader de decodificación
    }

} // namespace wivrn
