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
        layer_files('-'),
        create_hyper_file('-'),
        hyper_file_name(),
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
        video_res(),
        video_fps(),
        audio_output_extension("mp3"),
        runNumber(1),
        num_runs(-1),
        num_audio_channels(-1)
    {}

    std::vector<std::string> input_files;
    char layer_files;
    char create_hyper_file;
    std::string hyper_file_name;
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
    std::string video_res;
    std::string video_fps;
    std::string audio_output_extension;
    int runNumber;
    int num_runs;
    int num_audio_channels;
};

void get_other_config(YoloConfig *config, int *seed);
void run_yolo_process(YoloConfig *config, int seed);
std::string get_pan_filter_string(int num_input_channels, int num_output_channels);
std::vector<char*> get_argv(const std::string& cmd, std::vector<std::string>& storage);
void log_current_time(FILE *f) ;
bool is_video_file(const std::string& filename);
int get_audio_channel_count(const std::string& filename, FILE* log_file);
std::string get_basename(const std::string& path);
void print_help(const char* app_name);
template<typename T> void prompt_for_value (const std::string& prompt, T& value);
