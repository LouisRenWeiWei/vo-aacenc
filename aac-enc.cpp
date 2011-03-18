/* ------------------------------------------------------------------
 * Copyright (C) 2011 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <common/include/voAAC.h>
#include <common/include/cmnMemory.h>
#include "wavreader.h"

void usage(const char* name) {
	fprintf(stderr, "%s [-r bitrate] in.wav out.aac\n", name);
}

int main(int argc, char *argv[]) {
	int bitrate = 64000;
	int ch;
	while ((ch = getopt(argc, argv, "r:")) != -1) {
		switch (ch) {
		case 'r':
			bitrate = atoi(optarg);
			break;
		case '?':
		default:
			usage(argv[0]);
			return 1;
		}
	}
	if (argc - optind < 2) {
		usage(argv[0]);
		return 1;
	}
	const char* infile = argv[optind];
	const char* outfile = argv[optind + 1];

	WavReader wav(infile);
	int format, sampleRate, channels, bitsPerSample;
	if (!wav.getHeader(&format, &channels, &sampleRate, &bitsPerSample, NULL)) {
		fprintf(stderr, "Bad wav file %s\n", infile);
		return 1;
	}
	if (format != 1) {
		fprintf(stderr, "Unsupported WAV format %d\n", format);
		return 1;
	}
	if (bitsPerSample != 16) {
		fprintf(stderr, "Unsupported WAV sample depth %d\n", bitsPerSample);
		return 1;
	}
	int inputSize = channels*2*1024;
	uint8_t* inputBuf = new uint8_t[inputSize];
	int16_t* convertBuf = new int16_t[inputSize/2];

	VO_AUDIO_CODECAPI codec_api = { 0 };
	VO_HANDLE handle = 0;
	VO_MEM_OPERATOR mem_operator = { 0 };
	VO_CODEC_INIT_USERDATA user_data;
	voGetAACEncAPI(&codec_api);

	mem_operator.Alloc = cmnMemAlloc;
	mem_operator.Copy = cmnMemCopy;
	mem_operator.Free = cmnMemFree;
	mem_operator.Set = cmnMemSet;
	mem_operator.Check = cmnMemCheck;
	user_data.memflag = VO_IMF_USERMEMOPERATOR;
	user_data.memData = &mem_operator;
	codec_api.Init(&handle, VO_AUDIO_CodingAAC, &user_data);

	AACENC_PARAM params = { 0 };
	params.sampleRate = sampleRate;
	params.bitRate = bitrate;
	params.nChannels = channels;
	params.adtsUsed = 1;
	if (codec_api.SetParam(handle, VO_PID_AAC_ENCPARAM, &params) != VO_ERR_NONE) {
		fprintf(stderr, "Unable to set encoding parameters\n");
		return 1;
	}

	FILE* out = fopen(outfile, "wb");
	if (!out) {
		perror(outfile);
		return 1;
	}

	while (true) {
		VO_CODECBUFFER input = { 0 }, output = { 0 };
		VO_AUDIO_OUTPUTINFO output_info = { 0 };

		int read = wav.readData(inputBuf, inputSize);
		if (read < inputSize)
			break;
		for (int i = 0; i < read/2; i++) {
			const uint8_t* in = &inputBuf[2*i];
			convertBuf[i] = in[0] | (in[1] << 8);
		}
		input.Buffer = (uint8_t*) convertBuf;
		input.Length = read;
		codec_api.SetInputData(handle, &input);

		uint8_t outbuf[20480];
		output.Buffer = outbuf;
		output.Length = sizeof(outbuf);
		if (codec_api.GetOutputData(handle, &output, &output_info) != VO_ERR_NONE) {
			fprintf(stderr, "Unable to encode frame\n");
			return 1;
		}
		fwrite(outbuf, 1, output.Length, out);
	}
	delete [] inputBuf;
	delete [] convertBuf;
	fclose(out);
	codec_api.Uninit(handle);

	return 0;
}

