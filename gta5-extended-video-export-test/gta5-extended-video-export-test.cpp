// gta5-extended-video-export-test.cpp : Defines the entry point for the console application.
//

#include "../gta5-extended-video-export/encoder.h"
#include <iostream>

int main()
{
	av_register_all();
	avcodec_register_all();
	av_log_set_level(AV_LOG_TRACE);
	for (int j = 0; j < 10; j++) {
		std::shared_ptr<Encoder::Session> session(new Encoder::Session());
		session->createContext("mp4", ".\\test.mp4", ".\\", "movflags=+faststart", 1280, 720, "rgb24", 30000, 1001, 0, 0.0f, "yuv420p", "libx264", "", 2, 48000, 16, "s16", 3, "fltp", "aac", "ar=48000");
		for (int i = 0; i < 100; i++) {
			char* x = new char[1280 * 720 * 3];
			std::fill(x, x + (1280 * 720 * 3), i % 256);
			session->enqueueVideoFrame((BYTE*)x, 1280 * 720 * 3);
			session->writeAudioFrame((BYTE*)x, 1024, 0);
			delete[] x;
		}
		session.reset();
	}
	std::cin.get();
    return 0;
}