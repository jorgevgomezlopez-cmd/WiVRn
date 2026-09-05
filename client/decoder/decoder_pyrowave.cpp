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
    {
        std::cout << "[Pyrowave Client] Instanciado decodificador Pyrowave exitosamente." << std::endl;
    }

    void decoder_pyrowave::push_shard(uint64_t frame_index, std::span<const uint8_t> data) {
        if (data.empty()) return;

        static uint64_t counter = 0;
        if (++counter % 90 == 0) {
            std::cout << "[Pyrowave Client] Frame #" << frame_index
            << " recibido en APK | Bytes: " << data.size()
            << " | Muestra: " << (int)data[0] << " " << (int)data[1]
            << " " << (int)data[2] << " " << (int)data[3] << std::endl;
        }
    }

} // namespace wivrn
