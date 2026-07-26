#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdbool.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <limits>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE ProcessHandle;
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
typedef pid_t ProcessHandle;
#endif

#define MAX_FILES 100

struct YoloConfig {
    YoloConfig() :
        create_hyper_file(false),
        bass_boost(-1.0f),
        treble_gain(-1.0f),
        volume_lufs(-1.0f),
        tempo_modifier(-1.0f),
        atempo(0.0f),
        vtempo(0.0f),
        saturation(0.0f),
        contrast(0.0f),
        brightness(0.0f),
        saturationTarget(-1.0f),
        contrastTarget(-1.0f),
        brightnessTarget(-1.0f),
        volume(0.0f),
        bass(0.0f),
        treble(0.0f),
        quality(-1),
        video_output_extension("mkv"),
        audio_output_extension("mp3"),
        num_runs(-1),
        num_audio_channels(-1)
    {}

    std::vector<std::string> input_files;
    bool create_hyper_file;
    std::string hyper_filename;
    float bass_boost;
    float treble_gain;
    float volume_lufs;
    float tempo_modifier;
    float atempo;
    float vtempo;
    float saturation;
    float contrast;
    float brightness;
    float saturationTarget;
    float contrastTarget;
    float brightnessTarget;
    float volume;
    float bass;
    float treble;
    int quality;
    std::string video_output_extension;
    std::string audio_output_extension;
    int num_runs;
    int num_audio_channels;
};

void get_other_config(YoloConfig *config, int *seed);
void run_yolo_process(YoloConfig *config, int seed);
