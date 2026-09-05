#include "video_encoder_vulkan_pyrowave.h"
#include "utils/wivrn_vk_bundle.h"
#include <iostream>
#include "idr_handler.h"

namespace wivrn
{
    video_encoder_vulkan_pyrowave::video_encoder_vulkan_pyrowave(
        vk_bundle & vk,
        const encoder_settings & settings,
        uint8_t stream_idx)
    : video_encoder(
        vk,
        stream_idx,
        vk.queue.family_index,
        settings,
        std::make_unique<default_idr_handler>(), // <-- 1. Sin argumentos
                    true),
                    vk(vk)
                    {
                        std::cout << "[Pyrowave] Inicializando codificador por Compute Shaders para BC-250..." << std::endl;

                        vk::CommandPoolCreateInfo pool_info{};
                        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
                        pool_info.queueFamilyIndex = vk.queue.family_index;

                        command_pool = vk::raii::CommandPool(vk.device, pool_info);

                        init_compute_pipeline();

                        // Crear buffer de salida de 4MB por defecto para el flujo codificado
                        create_output_buffer(4 * 1024 * 1024);
                    }

                    void video_encoder_vulkan_pyrowave::create_output_buffer(size_t size)
                    {
                        output_buffer_size = size;

                        vk::BufferCreateInfo buffer_info{};
                        buffer_info.size = size;
                        buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer;
                        buffer_info.sharingMode = vk::SharingMode::eExclusive;

                        output_buffer = vk::raii::Buffer(vk.device, buffer_info);

                        vk::MemoryRequirements mem_reqs = output_buffer.getMemoryRequirements();

                        // Buscar tipo de memoria Visible/Coherente con la CPU
                        vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
                        vk::PhysicalDeviceMemoryProperties mem_properties = vk.physical_device.getMemoryProperties();

                        uint32_t memory_type_index = uint32_t(-1);
                        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
                        {
                            if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                                (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
                            {
                                memory_type_index = i;
                                break;
                            }
                        }

                        if (memory_type_index == uint32_t(-1))
                        {
                            throw std::runtime_error("[Pyrowave] No se encontró memoria HostVisible adecuada para el SSBO.");
                        }

                        vk::MemoryAllocateInfo alloc_info{};
                        alloc_info.allocationSize = mem_reqs.size;
                        alloc_info.memoryTypeIndex = memory_type_index;

                        output_memory = vk::raii::DeviceMemory(vk.device, alloc_info);

                        // LÍNEA CLAVE QUE FALTABA: Enlazar la memoria reservada con el buffer
                        output_buffer.bindMemory(*output_memory, 0);
                    }

                    void video_encoder_vulkan_pyrowave::reset()
                    {
                        // Método de reinicio para Pyrowave
                    }

                    void video_encoder_vulkan_pyrowave::init_compute_pipeline()
                    {
                        try
                        {
                            auto shader_module = vk.load_shader("pyrowave_encode");

                            std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {{
                                {
                                    .binding = 0,
                                    .descriptorType = vk::DescriptorType::eStorageImage,
                                    .descriptorCount = 1,
                                    .stageFlags = vk::ShaderStageFlagBits::eCompute
                                },
                                {
                                    .binding = 1,
                                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                                    .descriptorCount = 1,
                                    .stageFlags = vk::ShaderStageFlagBits::eCompute
                                }
                            }};

                            vk::DescriptorSetLayoutCreateInfo layout_info{};
                            layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
                            layout_info.pBindings = bindings.data();

                            descriptor_set_layout = vk::raii::DescriptorSetLayout(vk.device, layout_info);

                            vk::PushConstantRange push_constant_range{};
                            push_constant_range.stageFlags = vk::ShaderStageFlagBits::eCompute;
                            push_constant_range.offset = 0;
                            push_constant_range.size = sizeof(float);

                            vk::DescriptorSetLayout raw_layout = *descriptor_set_layout;
                            vk::PipelineLayoutCreateInfo pipeline_layout_info{};
                            pipeline_layout_info.setLayoutCount = 1;
                            pipeline_layout_info.pSetLayouts = &raw_layout;
                            pipeline_layout_info.pushConstantRangeCount = 1;
                            pipeline_layout_info.pPushConstantRanges = &push_constant_range;

                            pipeline_layout = vk::raii::PipelineLayout(vk.device, pipeline_layout_info);

                            vk::PipelineShaderStageCreateInfo stage_info{};
                            stage_info.stage = vk::ShaderStageFlagBits::eCompute;
                            stage_info.module = *shader_module;
                            stage_info.pName = "main";

                            vk::ComputePipelineCreateInfo pipeline_info{};
                            pipeline_info.stage = stage_info;
                            pipeline_info.layout = *pipeline_layout;

                            compute_pipeline = vk.device.createComputePipeline(nullptr, pipeline_info);

                            vk::DescriptorPoolSize pool_sizes[2] = {
                                { .type = vk::DescriptorType::eStorageImage, .descriptorCount = 1 },
                                { .type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1 }
                            };

                            vk::DescriptorPoolCreateInfo pool_info{};
                            pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
                            pool_info.maxSets = 1;
                            pool_info.poolSizeCount = 2;
                            pool_info.pPoolSizes = pool_sizes;

                            descriptor_pool = vk::raii::DescriptorPool(vk.device, pool_info);

                            vk::CommandPoolCreateInfo pool_create_info{};
                            pool_create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
                            pool_create_info.queueFamilyIndex = vk.queue.family_index;

                            command_pool = vk::raii::CommandPool(vk.device, pool_create_info);

                            vk::DescriptorSetAllocateInfo alloc_info{};
                            alloc_info.descriptorPool = *descriptor_pool;
                            alloc_info.descriptorSetCount = 1;
                            alloc_info.pSetLayouts = &raw_layout;

                            descriptor_sets = vk::raii::DescriptorSets(vk.device, alloc_info);

                            std::cout << "[Pyrowave] Descriptor Pool y Compute Pipeline listos!" << std::endl;
                        }
                        catch (const std::exception & e)
                        {
                            std::cerr << "[Pyrowave] Error configurando la Compute Pipeline: " << e.what() << std::endl;
                        }
                    }

                    // 1. Puente de 3 parámetros: Redirige la llamada de layer_commit hacia la implementación principal
                    void video_encoder_vulkan_pyrowave::present_image(
                        vk::Image y_cbcr,
                        vk::SemaphoreSubmitInfo sem_info,
                        uint64_t frame_index)
                    {
                        present_image(y_cbcr, sem_info, 0, frame_index);
                    }

                    void video_encoder_vulkan_pyrowave::present_image(
                        vk::Image y_cbcr,
                        vk::SemaphoreSubmitInfo sem_info,
                        uint8_t slot,
                        uint64_t frame_index)
                    {
                        try
                        {
                            // 1. Asegurar que el buffer de salida esté creado
                            if (!*output_buffer) {
                                create_output_buffer(4 * 1024 * 1024);
                            }

                            // 2. Crear la vista de la imagen de entrada
                            vk::ImageViewCreateInfo view_info{};
                            view_info.image = y_cbcr;
                            view_info.viewType = vk::ImageViewType::e2D;
                            view_info.format = vk::Format::eR8G8B8A8Unorm;
                            view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                            view_info.subresourceRange.baseMipLevel = 0;
                            view_info.subresourceRange.levelCount = 1;
                            view_info.subresourceRange.baseArrayLayer = 0;
                            view_info.subresourceRange.layerCount = 1;

                            vk::raii::ImageView input_view(vk.device, view_info);

                            // Binding 0: Imagen de Entrada
                            vk::DescriptorImageInfo image_info{};
                            image_info.imageView = *input_view;
                            image_info.imageLayout = vk::ImageLayout::eGeneral;

                            // Binding 1: SSBO de Salida
                            vk::DescriptorBufferInfo buffer_info{};
                            buffer_info.buffer = *output_buffer;
                            buffer_info.offset = 0;
                            buffer_info.range = VK_WHOLE_SIZE;

                            std::array<vk::WriteDescriptorSet, 2> writes{};
                            writes[0].dstSet = *descriptor_sets[0];
                            writes[0].dstBinding = 0;
                            writes[0].descriptorCount = 1;
                            writes[0].descriptorType = vk::DescriptorType::eStorageImage;
                            writes[0].pImageInfo = &image_info;

                            writes[1].dstSet = *descriptor_sets[0];
                            writes[1].dstBinding = 1;
                            writes[1].descriptorCount = 1;
                            writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
                            writes[1].pBufferInfo = &buffer_info;

                            vk.device.updateDescriptorSets(writes, nullptr);

                            // 3. Crear buffer de comandos desde el pool
                            vk::CommandBufferAllocateInfo alloc_info{};
                            alloc_info.commandPool = *command_pool;
                            alloc_info.level = vk::CommandBufferLevel::ePrimary;
                            alloc_info.commandBufferCount = 1;

                            vk::raii::CommandBuffers cmd_buffers(vk.device, alloc_info);
                            auto & cmd = cmd_buffers[0];

                            vk::CommandBufferBeginInfo begin_info{};
                            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

                            cmd.begin(begin_info);

                            vk::ImageMemoryBarrier2 image_barrier{};
                            image_barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput | vk::PipelineStageFlagBits2::eAllCommands;
                            image_barrier.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eMemoryWrite;
                            image_barrier.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader;
                            image_barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
                            image_barrier.oldLayout = vk::ImageLayout::eUndefined;
                            image_barrier.newLayout = vk::ImageLayout::eGeneral;
                            image_barrier.image = y_cbcr;
                            image_barrier.subresourceRange = view_info.subresourceRange;

                            vk::DependencyInfo dependency_info{};
                            dependency_info.imageMemoryBarrierCount = 1;
                            dependency_info.pImageMemoryBarriers = &image_barrier;

                            cmd.pipelineBarrier2(dependency_info);

                            cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *compute_pipeline);
                            cmd.bindDescriptorSets(
                                vk::PipelineBindPoint::eCompute,
                                *pipeline_layout,
                                0,
                                *descriptor_sets[0],
                                nullptr
                            );

                            float quant_step = 0.05f;
                            cmd.pushConstants(
                                *pipeline_layout,
                                vk::ShaderStageFlagBits::eCompute,
                                0,
                                sizeof(float),
                                              &quant_step
                            );

                            uint32_t dispatch_w = stream_width ? stream_width : 1920;
                            uint32_t dispatch_h = stream_height ? stream_height : 1080;

                            uint32_t group_x = (dispatch_w + 7) / 8;
                            uint32_t group_y = (dispatch_h + 7) / 8;
                            cmd.dispatch(group_x, group_y, 1);

                            cmd.end();

                            // 4. Submit e integración de semáforos de sincronización
                            vk::CommandBufferSubmitInfo cmd_submit_info{};
                            cmd_submit_info.commandBuffer = *cmd;

                            std::array<vk::SemaphoreSubmitInfo, 1> waits = {sem_info};

                            vk::SubmitInfo2 submit_info{};
                            submit_info.waitSemaphoreInfoCount = sem_info.semaphore ? 1u : 0u;
                            submit_info.pWaitSemaphoreInfos = sem_info.semaphore ? waits.data() : nullptr;
                            submit_info.commandBufferInfoCount = 1;
                            submit_info.pCommandBufferInfos = &cmd_submit_info;

                            // Enviar a la cola nativa de WiVRn
                            vk.queue.queue.submit2(submit_info);

                            // Esperar a nivel de Dispositivo para evitar punteros desreferenciados
                            vk.device.waitIdle();
                        }
                        catch (const std::exception & e)
                        {
                            std::cerr << "[Pyrowave] Error durante present_image: " << e.what() << std::endl;
                        }
                    }

                  /*  std::optional<video_encoder::data> video_encoder_vulkan_pyrowave::encode(
                        uint8_t slot,
                        uint64_t frame_index)
                    {
                        try
                        {
                            // Si por alguna razón la memoria no está mapeada, forzamos la creación/reserva
                            if (!*output_memory || output_buffer_size == 0)
                            {
                                create_output_buffer(4 * 1024 * 1024);
                            }

                            void* mapped_data = output_memory.mapMemory(0, output_buffer_size);

                            auto cpu_buffer = std::make_shared<std::vector<uint8_t>>(output_buffer_size);
                            if (mapped_data)
                            {
                                std::memcpy(cpu_buffer->data(), mapped_data, output_buffer_size);
                                output_memory.unmapMemory();
                            }

                            // Devolver SIEMPRE el objeto data para mantener satisfecho el ciclo de vida del encoder
                            video_encoder::data encoded_frame;
                            encoded_frame.encoder = this;
                            encoded_frame.span = std::span<uint8_t>(cpu_buffer->data(), cpu_buffer->size());
                            encoded_frame.mem = cpu_buffer;
                            encoded_frame.prefer_control = false;
                            std::cout << "[Pyrowave] Frame enviado: " << frame_index << " | Tamaño: " << cpu_buffer->size() << std::endl;

                            return encoded_frame;
                        }
                        catch (const std::exception & e)
                        {
                            std::cerr << "[Pyrowave] Error crítico en encode: " << e.what() << std::endl;
                            return std::nullopt;
                        }
                    }*/
                  std::optional<video_encoder::data> video_encoder_vulkan_pyrowave::encode(
                      uint8_t slot,
                      uint64_t frame_index)
                  {
                      try
                      {
                          constexpr size_t max_frame_size = 1024 * 1024; // 1 MB

                          if (!*output_memory || output_buffer_size == 0)
                          {
                              create_output_buffer(max_frame_size);
                          }

                          void* mapped_data = output_memory.mapMemory(0, output_buffer_size);

                          auto cpu_buffer = std::make_shared<std::vector<uint8_t>>(output_buffer_size);
                          if (mapped_data)
                          {
                              std::memcpy(cpu_buffer->data(), mapped_data, output_buffer_size);
                              output_memory.unmapMemory();
                          }
                          // Añadir justo después de la copia con memcpy en encode()
                          static uint64_t debug_counter = 0;
                          if (++debug_counter % 90 == 0) // Loguear cada 90 frames (~1 segundo)
                          {
                              uint8_t* ptr = cpu_buffer->data();
                              std::cout << "[Pyrowave Debug] Frame #" << frame_index
                              << " | Bytes copiados: " << cpu_buffer->size()
                              << " | Muestra de bytes [0..3]: "
                              << (int)ptr[0] << " " << (int)ptr[1] << " "
                              << (int)ptr[2] << " " << (int)ptr[3] << std::endl;
                          }

                          video_encoder::data encoded_frame;
                          encoded_frame.encoder = this;
                          encoded_frame.span = std::span<uint8_t>(cpu_buffer->data(), cpu_buffer->size());
                          encoded_frame.mem = cpu_buffer;
                          encoded_frame.prefer_control = false;

                          return encoded_frame;
                      }
                      catch (const std::exception & e)
                      {
                          std::cerr << "[Pyrowave] Error en encode: " << e.what() << std::endl;
                          return std::nullopt;
                      }
                  }
} // namespace wivrn
