#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdbool.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <limits>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

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
        remix_enabled('-'),
        remix_seed(0),
        remix_intensity(-1.0f),
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
        width(7680),
        height(4320),
        video_fps("480"),
        audio_output_extension("mp3"),
        audio_output_extension_is_default(true),
        runNumber_is_default(true),
        runNumber(1),
        num_runs(-1),
        num_audio_channels(-1)
    {}

    std::vector<std::string> input_files;
    char layer_files;
    char remix_enabled;
    int remix_seed;
    float remix_intensity;
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
    int width;
    int height;
    std::string video_fps;
    float fps = 480.0f;
    std::string audio_output_extension;
    bool audio_output_extension_is_default;
    bool runNumber_is_default;
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
std::vector<std::string> expand_wildcards(const std::string& path_pattern);

#define MAX_PROCESSES (MAX_FILES * 10)

// A simple thread pool for running tasks in parallel.
class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    template<class F, class... Args>
    void enqueue(F&& f, Args&&... args);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// Template implementation must be in the header file.
template<class F, class... Args>
void ThreadPool::enqueue(F&& f, Args&&... args) {
    auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.emplace(task);
    }
    condition.notify_one();
}
