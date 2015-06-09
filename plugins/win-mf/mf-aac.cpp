#include <obs-module.h>

#include <memory>

#include "mf-aac-encoder.hpp"

static const char *MFAAC_GetName(void)
{
	return obs_module_text("MFAACEnc");
}

static obs_properties_t *MFAAC_GetProperties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int(props, "bitrate",
			obs_module_text("Bitrate"), 32, 256, 32);

	return props;
}

static void MFAAC_GetDefaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 128);
}

static void *MFAAC_Create(obs_data_t *settings, obs_encoder_t *encoder)
{
	audio_t *audio = obs_encoder_audio(encoder);

	UINT32 bitrate = (UINT32)obs_data_get_int(settings, "bitrate");
	UINT32 channels = (UINT32)audio_output_get_channels(audio);
	UINT32 sampleRate = audio_output_get_sample_rate(audio);
	UINT32 bitsPerSample = 16;

	std::unique_ptr<MFAAC::Encoder> enc(new MFAAC::Encoder(
			bitrate, channels, sampleRate, bitsPerSample));

	if (!enc->Initialize())
		return nullptr;

	return enc.release();
}

static void MFAAC_Destroy(void *data)
{
	MFAAC::Encoder *enc = static_cast<MFAAC::Encoder *>(data);
	delete enc;
}

static bool MFAAC_Encode(void *data, struct encoder_frame *frame,
		struct encoder_packet *packet, bool *received_packet)
{
	MFAAC::Encoder *enc = static_cast<MFAAC::Encoder *>(data);
	MFAAC::Status status;

	if (!enc->ProcessInput(frame->data[0], frame->linesize[0], frame->pts,
			&status))
		return false;

	// This shouldn't happen since we drain right after
	// we process input
	if (status == MFAAC::NOT_ACCEPTING)
		return false;

	UINT8 *outputData;
	UINT32 outputDataLength;
	UINT64 outputPts;

	if (!enc->ProcessOutput(&outputData, &outputDataLength, &outputPts, 
			&status))
		return false;

	// Needs more input, not a failure case
	if (status == MFAAC::NEED_MORE_INPUT)
		return true;

	packet->pts = outputPts;
	packet->dts = outputPts;
	packet->data = outputData;
	packet->size = outputDataLength;
	packet->type = OBS_ENCODER_AUDIO;
	packet->timebase_num = 1;
	packet->timebase_den = enc->SampleRate();

	return *received_packet = true;
}

static bool MFAAC_GetExtraData(void *data, uint8_t **extra_data, size_t *size)
{
	MFAAC::Encoder *enc = static_cast<MFAAC::Encoder *>(data);

	UINT32 length;
	if (enc->ExtraData(extra_data, &length)) {
		*size = length;
		return true;
	}
	return false;
}

static void MFAAC_GetAudioInfo(void *data, struct audio_convert_info *info)
{
	UNUSED_PARAMETER(data);
	info->format = AUDIO_FORMAT_16BIT;
}

static size_t MFAAC_GetFrameSize(void *data)
{
	UNUSED_PARAMETER(data);
	return MFAAC::Encoder::FrameSize;
}

extern "C" {
void RegisterMFAACEncoder();
}

void RegisterMFAACEncoder() 
{
	obs_encoder_info info;
	info.id                        = "mf_aac";
	info.type                      = OBS_ENCODER_AUDIO;
	info.codec                     = "AAC";
	info.get_name                  = MFAAC_GetName;
	info.create                    = MFAAC_Create;
	info.destroy                   = MFAAC_Destroy;
	info.encode                    = MFAAC_Encode;
	info.get_frame_size            = MFAAC_GetFrameSize;
	info.get_defaults              = MFAAC_GetDefaults;
	info.get_properties            = MFAAC_GetProperties;
	info.get_extra_data            = MFAAC_GetExtraData;
	info.get_audio_info            = MFAAC_GetAudioInfo;

	obs_register_encoder(&info);
}
