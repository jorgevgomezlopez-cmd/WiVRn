/*
 * WiVRn VR streaming
 * Copyright (C) 2025 Patrick Nicolas <patricknicolas@laposte.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. See the <https://www.gnu.org/licenses/>.
 */

#include "decoder.h"

#ifdef __ANDROID__
#include "decoder/android/android_decoder.h"
#else
#include "decoder/ffmpeg/ffmpeg_decoder.h"
#endif
#include "decoder/raw_decoder.h"

// Declaración del decodificador Pyrowave
#include "decoder_pyrowave.h"

wivrn::decoder::~decoder() = default;

std::shared_ptr<wivrn::decoder> wivrn::decoder::make(
	vk::raii::Device & device,
	vk::raii::PhysicalDevice & phys_dev,
	uint32_t vk_queue_family_index,
	const wivrn::to_headset::video_stream_description & description,
	uint8_t stream_index,
	std::weak_ptr<scenes::stream> scene,
	shard_accumulator * acc)
{
	// Instancia directa del decodificador Pyrowave
	return std::make_shared<wivrn::decoder_pyrowave>(
		device,
		phys_dev,
		vk_queue_family_index,
		description,
		stream_index,
		scene,
		acc);
}

static std::vector<wivrn::video_codec> supported_codecs_()
{
	std::vector<wivrn::video_codec> res;
	#ifdef __ANDROID__
	wivrn::android::decoder::supported_codecs(res);
	#else
	wivrn::ffmpeg::decoder::supported_codecs(res);
	#endif
	res.push_back(wivrn::video_codec::raw);

	// Registra Pyrowave en la lista para que la app y el servidor sepan que está disponible
	res.push_back(wivrn::video_codec::pyrowave);

	return res;
}

const std::vector<wivrn::video_codec> & wivrn::decoder::supported_codecs()
{
	static std::vector<wivrn::video_codec> res = supported_codecs_();
	return res;
}
