#include <algorithm>
#include <functional>
#include <iostream>

#include "pitch_detection/pitch_detection.h"
#include <gflags/gflags.h>
#include <libnyquist/Decoders.h>

DEFINE_double(timeslice, 0.01, "Time slice");

template <class T>
std::vector<T>
get_chunks(T container, size_t k)
{
	std::vector<T> ret;

	size_t size = container.size();
	size_t i = 0;

	if (size > k) {
		for (; i < size - k; i += k) {
			ret.push_back(T(container.begin() + i, container.begin() + i + k));
		}
	}

	if (i % k) {
		ret.push_back(
		    T(container.begin() + i, container.begin() + i + (i % k)));
	}

	return ret;
}

int
main(int argc, char **argv)
{
	gflags::SetUsageMessage("help\n");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	std::cout << "Slicing wav file into chunks of " << FLAGS_timeslice
	          << " seconds..." << std::endl;

	nqr::NyquistIO loader;

	if (argc != 2) {
		std::cerr << "Usage: wav_analyzer /path/to/audio/file" << std::endl;
		return -1;
	}

	std::shared_ptr<nqr::AudioData> file_data =
	    std::make_shared<nqr::AudioData>();
	loader.Load(file_data.get(), argv[1]);

	std::cout << "Audio file info:" << std::endl;

	std::cout << "\tsample rate: " << file_data->sampleRate << std::endl;
	std::cout << "\tlen samples: " << file_data->samples.size() << std::endl;
	std::cout << "\tframe size: " << file_data->frameSize << std::endl;
	std::cout << "\tseconds: " << file_data->lengthSeconds << std::endl;
	std::cout << "\tchannels: " << file_data->channelCount << std::endl;

	std::transform(file_data->samples.begin(), file_data->samples.end(),
	    file_data->samples.begin(),
	    std::bind(std::multiplies<float>(), std::placeholders::_1, 10000));

	std::vector<float> audio;

	if (file_data->channelCount == 2) {
		// convert stereo to mono
		std::vector<float> audio_copy(file_data->samples.size() / 2);
		nqr::StereoToMono(file_data->samples.data(), audio_copy.data(),
		    file_data->samples.size());
		audio = std::vector<float>(audio_copy.begin(), audio_copy.end());
	} else {
		audio = std::vector<float>(
		    file_data->samples.begin(), file_data->samples.end());
	}

	auto chunk_size = size_t(double(audio.size()) * FLAGS_timeslice);
	std::cout << "Slicing buffer size " << audio.size()
	          << " into chunks of size " << chunk_size << std::endl;

	auto chunks = get_chunks(audio, chunk_size);
	auto sample_rate = file_data->sampleRate;

	double t = 0.;

	for (auto chunk : chunks) {
		std::cout << "At t: " << t << std::endl;

		auto pitch_mpm = pitch::mpm<float>(chunk, sample_rate);
		auto pitch_yin = pitch::yin<float>(chunk, sample_rate);
		auto pitch_swipe = pitch::swipe<float>(chunk, sample_rate);

		std::cout << "\tmpm: " << pitch_mpm << "\n\tyin: " << pitch_yin
		          << "\n\tswipe: " << pitch_swipe << std::endl;

		auto pitches_mpm = pitch::pmpm<float>(chunk, sample_rate);
		auto pitches_yin = pitch::pyin<float>(chunk, sample_rate);

		for (auto p_mpm : pitches_mpm) {
			std::cout << "\tpmpm: f0: " << p_mpm.first
			          << ", p: " << p_mpm.second << std::endl;
		}

		for (auto p_yin : pitches_yin) {
			std::cout << "\tpyin: f0: " << p_yin.first
			          << ", p: " << p_yin.second << std::endl;
		}

		t += FLAGS_timeslice;
	}

	return 0;
}
