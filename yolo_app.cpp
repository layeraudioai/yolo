#include "yolo_core.hpp"
#include <sstream>
#include <iterator>
#include <iomanip>
#include <algorithm>
#include <iostream>
#define MAX_PROCESSES (MAX_FILES * 10)

bool is_video_file(const std::string& filename);

std::string get_pan_filter_string(int num_input_channels, int num_output_channels) {
    if (num_input_channels <= 0 || num_output_channels <= 0) return "";

    std::string pan_str = "pan=";
    if (num_output_channels == 1) {
        pan_str += "mono|c0=c0";
    } else {
        // The layout is determined by the number of *output* channels.
        std::string layout;
        if (num_output_channels == 2) layout = "stereo";
        else if (num_output_channels == 6) layout = "5.1";
        else if (num_output_channels == 8) layout = "7.1";
        else if (num_output_channels == 16) layout = "hexadecagonal";
        else if (num_output_channels == 24) layout = "22.2";
        else layout = std::to_string(num_output_channels);

        pan_str += layout;
        // Define each output channel (c0, c1, ... c<num_output_channels-1>)
        for (int i = 0; i < num_output_channels; ++i) {
            pan_str += "|c" + std::to_string(i) + "=";
            bool first_mix = true;
            // Mix from the available *input* channels (c0, c1, ... c<num_input_channels-1>)
            for (int j = 0; j < num_input_channels; ++j) {
                if (rand() % 2 == 0) { // Randomly decide to mix from channel j
                    if (!first_mix) pan_str += "+";
                    pan_str += "c" + std::to_string(j);
                    first_mix = false;
                }
            }
            // Ensure the channel definition is not empty
            if (first_mix) pan_str += "c" + std::to_string(i % num_input_channels);
        }
    }
    return pan_str;
}

// Template for reading any type that supports `std::cin >>`
template<typename T>
void prompt_for_value(const std::string& prompt, T& value) {
    while (true) {
        std::cout << prompt;
        std::cin >> value;
        if (std::cin.good()) {
            // Clear the rest of the line from the input buffer
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            break;
        }
        std::cout << "Invalid input. Please try again.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

// Overload for std::string to correctly read entire lines, including spaces.
void prompt_for_value(const std::string& prompt, std::string& value) {
    std::cout << prompt;
    std::getline(std::cin, value);
}

void get_other_config(YoloConfig *config, int *seed) {
    if (config->layer_files=='-') {
        prompt_for_value("Generate layered files?: ", config->layer_files);
    }
    if (config->create_hyper_file=='-') {
        prompt_for_value("Create a hyper file?: ", config->create_hyper_file);
    }
    if (config->create_hyper_file=='y') {
        prompt_for_value("Hyper file name?: ", config->hyper_file_name);
    }
    std::cout << "Audio options:\n";
    if (config->bass_boost == -1.0f)
        prompt_for_value("Enter bass boost: ", config->bass_boost);
    if (config->treble_gain == -1.0f)
        prompt_for_value("Enter treble gain: ", config->treble_gain);
    if (config->volume_lufs == -1.0f)
        prompt_for_value("Enter volume LUFS: ", config->volume_lufs);
    if (config->tempo_modifier == -1.0f)
        prompt_for_value("Enter tempo modifier: ", config->tempo_modifier);
    if (config->quality == -1)
        prompt_for_value("Enter quality (0[max]-31[lowest]): ", config->quality);
    if (config->num_audio_channels == -1)
        prompt_for_value("Enter number of output audio channels: ", config->num_audio_channels);

    // Check if there are any video files before asking for video-specific options.
    bool has_video_files = false;
    for (const auto& file : config->input_files) {
        if (is_video_file(file)) {
            has_video_files = true;
            break;
        }
    }
    if (has_video_files && config->video_output_extension == "mkv") { // Not set by user arg
        std::cout << "Video specific options:\n";
        if (config->brightnessTarget == -1.0f)
            prompt_for_value("Enter brightness target: ", config->brightnessTarget);
        if (config->contrastTarget == -1.0f)
            prompt_for_value("Enter contrast target: ", config->contrastTarget);
        if (config->saturationTarget == -1.0f)
            prompt_for_value("Enter saturation target: ", config->saturationTarget);
        prompt_for_value("Enter video output extension (e.g., mkv, mp4): ", config->video_output_extension);
    }
    // Always prompt for audio extension if not set by user arg
    if (config->audio_output_extension == "mp3")
        prompt_for_value("Enter audio output extension (e.g., mp3, ogg): ", config->audio_output_extension);
    if (config->num_runs == -1)
        prompt_for_value("Enter number of runs: ", config->num_runs);
    if (*seed == 0)
        prompt_for_value("Enter random seed (0 for random): ", *seed);
    if (*seed != 0)
        prompt_for_value("Starting run number: ", config->runNumber);
    else 
        config->runNumber = 1;
}

void log_current_time(FILE *f) {
    time_t now = time(NULL);
#ifdef _WIN32
    struct tm t;
    localtime_s(&t, &now);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
#else
    struct tm *t = localtime(&now);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
#endif
    fprintf(f, "[%s] ", buf);
}

#ifndef _WIN32
// This function is currently unused.
std::vector<char*> get_argv(const std::string& cmd, std::vector<std::string>& storage) {
    std::istringstream iss(cmd);
    storage.assign(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{});

    std::vector<char*> argv;
    for (auto& s : storage) {
        argv.push_back(&s[0]);
    }
    argv.push_back(nullptr);
    return argv;
}
#endif


// Helper function to check for common video file extensions (case-insensitive)
bool is_video_file(const std::string& filename) {
    size_t dot_pos = filename.find_last_of(".");
    if (dot_pos == std::string::npos) {
        return false; // No extension
    }

    std::string ext = filename.substr(dot_pos + 1);
    // Convert to lower case for comparison
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    const std::vector<std::string> video_extensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "mpeg", "mpg"
    };

    for (const auto& video_ext : video_extensions) {
        if (ext == video_ext) {
            return true;
        }
    }
    return false;
}

#ifdef _WIN32
// Define the path to ffmpeg.exe. This is more robust than relying on PATH.
const char* FFMPEG_PATH = "ffmpeg.exe";
const char* FFPROBE_PATH = "ffprobe.exe";
#else
const char* FFMPEG_PATH = "ffmpeg";
const char* FFPROBE_PATH = "ffprobe";
#endif

// Helper to extract filename from a full path
std::string get_basename(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        return path.substr(last_slash + 1);
    }
    return path;
}

// Helper function to get the number of audio channels from a file using ffprobe
int get_audio_channel_count(const std::string& filename, FILE* log_file) {
    std::string command = std::string(FFPROBE_PATH) + " -v error -select_streams a:0 -show_entries stream=channels -of default=noprint_wrappers=1:nokey=1 \"" + filename + "\"";
    int channels = 0;

#ifdef _WIN32
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        fprintf(log_file, "  ffprobe CreatePipe failed.\n");
        return 2; // Fallback
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmd_line(command.begin(), command.end());
    cmd_line.push_back('\0');

    if (!CreateProcessA(NULL, cmd_line.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(log_file, "  ffprobe CreateProcess failed (%ld).\n", GetLastError());
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 2; // Fallback
    }

    CloseHandle(hWrite); // Close the write end of the pipe in the parent

    char buffer[128];
    DWORD bytesRead;
    std::string output;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead != 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    try {
        if (!output.empty()) {
            channels = std::stoi(output);
        }
    } catch (const std::exception& e) {
        fprintf(log_file, "  ffprobe failed to parse channel count for '%s'. Output: %s\n", filename.c_str(), output.c_str());
    }

    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else // POSIX popen
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        fprintf(log_file, "  ffprobe popen failed.\n");
        return 2; // Fallback
    }
    char buffer[16];
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        channels = atoi(buffer);
    }
    pclose(pipe);
#endif

    return (channels > 0) ? channels : 2; // Fallback to 2 if detection fails or returns 0
}

void run_yolo_process(YoloConfig *config, int seed) {

    FILE *log_file = fopen("yolo.log", "a");
    if (!log_file) {
        fprintf(stderr, "Failed to open yolo.log for writing.\n");
        return;
    }

    fprintf(log_file, "--- Yolo Process Started (Seed: %d) ---\n", seed);
    fprintf(log_file, "Configuration:\n");
    fprintf(log_file, "  Number of Runs: %d\n", config->num_runs);
    fprintf(log_file, "----------------------------\n");

    ProcessHandle processes[MAX_PROCESSES];
    int process_count = 0;

    for (int run = 0; run < config->num_runs; run++) {
        // Re-seed the RNG for each run to get different random values.
        // Using seed + run makes the results for each run deterministic and reproducible.
        unsigned int current_seed = (seed == 0) ? (unsigned int)time(NULL) + run : seed + run;
        srand(current_seed);

        // Recalculate randomized parameters for each run
        config->brightness = config->brightnessTarget + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->contrast = config->contrastTarget + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->saturation = config->saturationTarget + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->volume = config->volume_lufs + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->treble = config->treble_gain + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->bass = config->bass_boost + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->atempo = config->tempo_modifier + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->vtempo = 1.0 / config->atempo;

        log_current_time(log_file);
        fprintf(log_file, "Starting Run %d with Seed %u\n", run, current_seed);

        if (config->layer_files) {
            // --- Layered files logic ---
            // A single ffmpeg command per run, with all files as inputs.

            // Check if there are any video files to determine the output container.
            bool has_video_input = false;
            for (const auto& file : config->input_files) {
                if (is_video_file(file)) {
                    has_video_input = true;
                    break;
                }
            }

            // Determine output filename and extension
            const std::string& output_ext = has_video_input ? config->video_output_extension : config->audio_output_extension;
            char output_filename[512];
            snprintf(output_filename, sizeof(output_filename), "layered_output_run%d.%s", run, output_ext.c_str());

            // Build input string: -i "file1" -i "file2" ...
            std::string input_str;
            for (const auto& file : config->input_files) {
                input_str += "-i \"" + file + "\" ";
            }

            // Build filter_complex string
            std::stringstream filter_complex;
            std::vector<int> audio_stream_indices;
            std::vector<int> video_stream_indices;

            for (size_t i = 0; i < config->input_files.size(); ++i) {
                audio_stream_indices.push_back(i); // Assume every input has an audio stream
                if (is_video_file(config->input_files[i])) {
                    video_stream_indices.push_back(i);
                }
            }

            // --- Correctly determine total input channels ---
            int total_input_channels = 0;
            log_current_time(log_file);
            fprintf(log_file, "Run %d: Detecting input channel counts for layering...\n", run);
            for (const auto& file : config->input_files) {
                int file_channels = get_audio_channel_count(file, log_file);
                fprintf(log_file, "  - '%s': %d channels\n", file.c_str(), file_channels);
                total_input_channels += file_channels;
            }
            fprintf(log_file, "  Total input channels for pan filter: %d\n", total_input_channels);
            fflush(log_file);

            // Audio chain
            for (int index : audio_stream_indices) {
                filter_complex << "[" << index << ":a]";
            }
            filter_complex << "amerge=inputs=" << audio_stream_indices.size() << "[a_merged];";
            std::string pan_filter = get_pan_filter_string(total_input_channels, config->num_audio_channels);
            filter_complex << "[a_merged]atempo=" << config->atempo
                           << ",volume=" << config->volume
                           << ",bass=gain=" << config->bass
                           << ",treble=gain=" << config->treble
                           << "," << pan_filter << "[a_out];";

            // Video chain
            if (!video_stream_indices.empty()) {
                if (video_stream_indices.size() > 1) {
                    filter_complex << "[" << video_stream_indices[0] << ":v][" << video_stream_indices[1] << ":v]blend=all_mode=multiply[v_blended];";
                    for (size_t i = 2; i < video_stream_indices.size(); ++i) {
                        filter_complex << "[v_blended][" << video_stream_indices[i] << ":v]blend=all_mode=multiply[v_blended];";
                    }
                    filter_complex << "[v_blended]setpts=" << config->vtempo << "*PTS,eq=brightness=" << config->brightness
                                   << ":contrast=" << config->contrast << ":saturation=" << config->saturation << "[v_out]";
                } else { // Only one video
                    filter_complex << "[" << video_stream_indices[0] << ":v]setpts=" << config->vtempo << "*PTS,eq=brightness=" << config->brightness
                                   << ":contrast=" << config->contrast << ":saturation=" << config->saturation << "[v_out]";
                }
            }

            // Build the full command
            std::string cmd_str = std::string(FFMPEG_PATH) + " " + input_str +
                                  "-filter_complex \"" + filter_complex.str() + "\"";

            if (has_video_input) {
                cmd_str += " -map \"[v_out]\" -q:v " + std::to_string(config->quality);
            }

            cmd_str += " -map \"[a_out]\"";

            // Handle audio options, especially the MP3 channel constraint.
            if (config->audio_output_extension == "mp3") {
                // MP3 must be 2 channels, regardless of what the pan filter did.
                cmd_str += " -q:a " + std::to_string(config->quality) + " -ac 2";
            } else {
                // For other formats, use the user-specified channel count.
                cmd_str += " -q:a " + std::to_string(config->quality) + " -ac " + std::to_string(config->num_audio_channels);
            }

            cmd_str += " -y \"";
            cmd_str += output_filename;
            cmd_str += "\"";

            log_current_time(log_file);
            fprintf(log_file, "Run %d: Launching layered ffmpeg process\n", run);
            fprintf(log_file, "  Command: %s\n", cmd_str.c_str());
            fflush(log_file);

#ifdef _WIN32
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            std::vector<char> cmd_line(cmd_str.begin(), cmd_str.end());
            cmd_line.push_back('\0');

            if (CreateProcess(NULL, cmd_line.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                processes[process_count++] = pi.hProcess;
                CloseHandle(pi.hThread);
            } else {
                fprintf(log_file, "  CreateProcess failed (%ld).\n", GetLastError());
            }
#else // POSIX fork/exec
            pid_t pid = fork();
            if (pid == 0) { // Child process
                execl("/bin/sh", "sh", "-c", cmd_str.c_str(), (char *) NULL);
                perror("execl failed");
                exit(1);
            } else if (pid > 0) { // Parent process
                processes[process_count++] = pid;
            } else { // Fork failed
                fprintf(log_file, "  fork failed.\n");
            }
#endif
        } else {
            // --- Original per-file logic ---
            for (size_t i = 0; i < config->input_files.size(); i++) {
                bool is_video = is_video_file(config->input_files[i]);
                const std::string& output_ext = is_video ? config->video_output_extension : config->audio_output_extension;
                std::string base_filename = get_basename(config->input_files[i]);
                char output_filename[512];
                // Pass the filename as an argument to snprintf to avoid format string vulnerabilities.
                snprintf(output_filename, sizeof(output_filename), "%s_run%d_file%zu.%s",
                         base_filename.c_str(), run, i, output_ext.c_str());

                char filter_complex_a[4096];
                char filter_complex_v[512];
                // Correctly get the channel count of the *input* file for the pan filter.
                int input_channels = get_audio_channel_count(config->input_files[i], log_file);
                std::string pan_filter = get_pan_filter_string(input_channels, config->num_audio_channels);
                snprintf(filter_complex_a, sizeof(filter_complex_a),
                    "atempo=%.6f,volume=%.6f,bass=gain=%.6f,treble=gain=%.6f,%s", // pan_filter is safe
                    config->atempo,
                    config->volume,
                    config->bass,
                    config->treble,
                    pan_filter.c_str());

                std::string cmd_str;
                if (is_video) {
                    snprintf(filter_complex_v, sizeof(filter_complex_v), "setpts=%.6f*PTS,eq=brightness=%.6f:contrast=%.6f:saturation=%.6f",
                             config->vtempo,
                             config->brightness,
                             config->contrast,
                             config->saturation);

                    std::string audio_codec_str;
                    if (config->num_audio_channels > 2) {
                        if (config->num_audio_channels <= 8) {
                            audio_codec_str = " -c:a flac"; // Use FLAC for up to 7.1 surround
                        } else {
                            audio_codec_str = " -c:a pcm_s24le"; // Use PCM for high channel counts
                        }
                    } else {
                        audio_codec_str = " -q:a " + std::to_string(config->quality); // Use quality for stereo/mono
                    }

                    if (config->create_hyper_file=='y') { cmd_str = std::string(FFMPEG_PATH) + " -i \"" + config->input_files[i] + "\"" +
                              " -filter_complex:a \"" + std::string(filter_complex_a) + "\"" +
                              " -filter_complex:v \"" + std::string(filter_complex_v) + "\"";
                              cmd_str += " -q:v " + std::to_string(config->quality) +
                              audio_codec_str +
                              " -ac " + std::to_string(config->num_audio_channels) +
                              " -y \"" + output_filename + "\" \"" + config->hyper_file_name + "\""; }
                    else { cmd_str = std::string(FFMPEG_PATH) + " -i \"" + config->input_files[i] + "\"" +
                              " -filter_complex:a \"" + std::string(filter_complex_a) + "\"" +
                              " -filter_complex:v \"" + std::string(filter_complex_v) + "\"";
                              cmd_str += " -q:v " + std::to_string(config->quality) +
                              audio_codec_str +
                              " -ac " + std::to_string(config->num_audio_channels) +
                              " -y \"" + output_filename + "\""; }
                } else { // Audio-only file
                    std::string audio_opts_str;
                    if (config->audio_output_extension == "mp3") {
                        // For MP3, use the user's channel count for the pan filter,
                        // but force the final output to 2 channels.
                        audio_opts_str = " -q:a " + std::to_string(config->quality) + " -ac 2";
                    } else if (config->audio_output_extension == "wav") {
                        // For WAV, use uncompressed PCM to support high channel counts.
                        audio_opts_str = " -c:a pcm_s24le -ac " + std::to_string(config->num_audio_channels);
                    } else if (config->audio_output_extension == "opus"){
                        audio_opts_str = " -ab " + std::to_string((256 * config->num_audio_channels) - (8 * config->quality)) +
                                         " -ac " + std::to_string(config->num_audio_channels);
                    } else {
                        // Default behavior for other formats (ogg, flac, etc.)
                        audio_opts_str = " -q:a " + std::to_string(config->quality) +
                                         " -ac " + std::to_string(config->num_audio_channels);
                    }

                    if (config->create_hyper_file=='y') { cmd_str = std::string(FFMPEG_PATH) + " -i \"" + 
                              config->input_files[i] + "\"" + " -af \"" + 
                              std::string(filter_complex_a) + "\"" + audio_opts_str + " -y \"" + output_filename + 
                              "\" \"" + config->hyper_file_name + "\""; }
                    else { cmd_str = std::string(FFMPEG_PATH) + " -i \"" + config->input_files[i] + "\"" +
                              " -af \"" + std::string(filter_complex_a) + "\"" +
                              audio_opts_str + " -y \"" + output_filename + "\""; }
                }

                log_current_time(log_file);
                fprintf(log_file, "Run %d: Launching ffmpeg for input: '%s'\n", run, config->input_files[i].c_str());            fprintf(log_file, "  Command: %s\n", cmd_str.c_str());
                fflush(log_file);

#ifdef _WIN32
                STARTUPINFO si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));

                // CreateProcess may modify the command line string, so we need a mutable buffer.
                std::vector<char> cmd_line(cmd_str.begin(), cmd_str.end());
                cmd_line.push_back('\0');

                if (CreateProcess(NULL, cmd_line.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                    processes[process_count++] = pi.hProcess;
                    CloseHandle(pi.hThread);
                } else {
                    fprintf(log_file, "  CreateProcess failed (%ld).\n", GetLastError());
                }
#else // POSIX fork/exec
                pid_t pid = fork();            if (pid == 0) { // Child process
                    execl("/bin/sh", "sh", "-c", cmd_str.c_str(), (char *) NULL);
                    // If execl returns, it must have failed.
                    perror("execl failed");
                    exit(1);
                } else if (pid > 0) { // Parent process
                    processes[process_count++] = pid;
                } else { // Fork failed
                    fprintf(log_file, "  fork failed.\n");
                }
#endif
            }
        }
    }
    
    for (int i = 0; i < process_count; i++) {
#ifdef _WIN32
        WaitForSingleObject(processes[i], INFINITE);
        CloseHandle(processes[i]);
#else
        int status;
        waitpid(processes[i], &status, 0);
#endif
    }
    
    if (config->create_hyper_file) {
        FILE *list = fopen("list.txt", "w");
        if (list) {
            for (int run = config->runNumber; run < config->num_runs+config->runNumber; run++) {
                for (size_t i = 0; i < config->input_files.size(); i++) {
                    if (config->layer_files) {
                        bool has_video = is_video_file(config->input_files[0]); // Simplified check, assumes homogeneity or first file is representative
                        const std::string& output_ext = has_video ? config->video_output_extension : config->audio_output_extension;
                        fprintf(list, "file 'layered_output_run%d.%s'\n", run, output_ext.c_str());
                        break;
                    } else {
                        bool is_video = is_video_file(config->input_files[i]);
                        const std::string& output_ext = is_video ? config->video_output_extension : config->audio_output_extension;
                        std::string base_filename = get_basename(config->input_files[i]);
                        fprintf(list, "file '%s_run%d_file%zu.%s'\n",
                                base_filename.c_str(), run, i, output_ext.c_str());
                    }
                }
            }
            fclose(list);
        }
        
        std::string concat_cmd_str = std::string(FFMPEG_PATH) + " -f concat -safe 0 -i list.txt -c copy \"" + config->hyper_file_name + "\"";

        if (!config->hyper_file_name.empty()) {
            log_current_time(log_file);
            fprintf(log_file, "Launching ffmpeg concatenation.\n");
            fprintf(log_file, "  Command: %s\n", concat_cmd_str.c_str());
            fflush(log_file);

#ifdef _WIN32
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            
            std::vector<char> concat_cmd_line(concat_cmd_str.begin(), concat_cmd_str.end());
            concat_cmd_line.push_back('\0');

            if (CreateProcess(NULL, concat_cmd_line.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
#else // POSIX fork/exec
            pid_t pid = fork();            if (pid == 0) { // Child
                execl("/bin/sh", "sh", "-c", concat_cmd_str.c_str(), (char *) NULL);
                perror("execl failed");
                exit(1);
            } else if (pid > 0) { // Parent
                int status;
                waitpid(pid, &status, 0);
            } else {
                fprintf(log_file, "  fork failed for concatenation.\n");
            }
#endif
        }
    }
    fclose(log_file);
}

void print_help(const char* app_name) {
    std::cout << "Usage: " << app_name << " [options] [input_file1 input_file2 ...]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --layer-files               Layer all inputs into a single output file per run.\n";
    std::cout << "  -h, --help                  Show this help message.\n";
    std::cout << "  --create-hyper-file         Flag to create a concatenated hyper file.\n";
    std::cout << "  --hyper-file_name <path>     Set the output name for the hyper file.\n";
    std::cout << "  -bt, --brightness-target <f>  Set the target brightness (float).\n";
    std::cout << "  -ct, --contrast-target <f>    Set the target contrast (float).\n";
    std::cout << "  -st, --saturation-target <f>  Set the target saturation (float).\n";
    std::cout << "  -bb, --bass-boost <f>         Set the bass boost gain (float).\n";
    std::cout << "  -tg, --treble-gain <f>        Set the treble gain (float).\n";
    std::cout << "  -l, --lufs <f>                Set the target volume in LUFS (float).\n";
    std::cout << "  -t, --tempo <f>               Set the tempo modifier (float).\n";
    std::cout << "  -q, --quality <int>           Set the output quality (0-31).\n";
    std::cout << "  --video-ext <ext>           Set the output extension for video files (default: mkv).\n";
    std::cout << "  --audio-ext <ext>           Set the output extension for audio files (default: mp3).\n";
    std::cout << "  -r, --runs <int>              Set the number of processing runs.\n";
    std::cout << "  -c, --channels <int>          Set the number of output audio channels.\n";
    std::cout << "  -s, --seed <int>              Set the random seed (0 for random).\n";
    std::cout << "\nIf options are not provided, you will be prompted for them interactively.\n";
}

int main(int argc, char *argv[]) {
    YoloConfig config;
    int seed = 0;

    // Print the YOLO ASCII logo using a raw string literal for correctness and clarity.
    std::cout << R"EOF(
             .__           
___.__. ____ |  |   ____ 
   |  |/  _ \|  |  /  _ \
\___  (  <_> )  |_(  <_> )
/ ____|\____/|____/\____/ 
\/                    
)EOF" << std::endl;

    // --- Argument Parsing ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "man") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--layer-files") {
            config.layer_files = true;
        } else if (arg == "--create-hyper-file") {
            config.create_hyper_file = true;
        } else if ((arg == "--hyper-filename" || arg == "--hyper") && i + 1 < argc) {
            config.hyper_file_name = argv[++i];
            config.create_hyper_file = true;
        } else if ((arg == "-bt" || arg == "--brightness-target") && i + 1 < argc) {
            config.brightnessTarget = std::stof(argv[++i]);
        } else if ((arg == "-ct" || arg == "--contrast-target") && i + 1 < argc) {
            config.contrastTarget = std::stof(argv[++i]);
        } else if ((arg == "-st" || arg == "--saturation-target") && i + 1 < argc) {
            config.saturationTarget = std::stof(argv[++i]);
        } else if ((arg == "-bb" || arg == "--bass-boost") && i + 1 < argc) {
            config.bass_boost = std::stof(argv[++i]);
        } else if ((arg == "-tg" || arg == "--treble-gain") && i + 1 < argc) {
            config.treble_gain = std::stof(argv[++i]);
        } else if ((arg == "-l" || arg == "--lufs") && i + 1 < argc) {
            config.volume_lufs = std::stof(argv[++i]);
        } else if ((arg == "-t" || arg == "--tempo") && i + 1 < argc) {
            config.tempo_modifier = std::stof(argv[++i]);
        } else if ((arg == "-q" || arg == "--quality") && i + 1 < argc) {
            config.quality = std::stoi(argv[++i]);
        } else if (arg == "--video-ext" && i + 1 < argc) {
            config.video_output_extension = argv[++i];
        } else if (arg == "--audio-ext" && i + 1 < argc) {
            config.audio_output_extension = argv[++i];
        } else if ((arg == "-c" || arg == "--channels") && i + 1 < argc) {
            config.num_audio_channels = std::stoi(argv[++i]);
        } else if ((arg == "-s" || arg == "--seed") && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        } else if (arg[0] == '-') {
            std::cerr << "Warning: Unknown option '" << arg << "' ignored." << std::endl;
        } else {
            // Treat as input file
            if (config.input_files.size() < MAX_FILES) {
                config.input_files.push_back(argv[i]);
            } else {
                std::cerr << "Warning: Maximum number of input files (" << MAX_FILES << ") reached. Ignoring '" << arg << "'." << std::endl;
            }
        }
    }

    // --- Input File Handling ---
    if (config.input_files.empty()) {
        // No files provided via args, prompt the user
        std::cout << "No input files provided. Please enter one or more file paths." << std::endl;
        std::cout << "Use quotes for paths with spaces (e.g., \"C:\\My Videos\\file 1.mp4\")." << std::endl;
        std::cout << "Enter an empty line when finished." << std::endl;
        std::string line;
        while (config.input_files.size() < MAX_FILES) {
            std::cout << "> ";
            std::getline(std::cin, line);
            if (line.empty()) {
                break;
            }

            size_t i = 0;
            while (i < line.length() && config.input_files.size() < MAX_FILES) {
                // Skip leading whitespace
                while (i < line.length() && isspace(line[i])) {
                    i++;
                }
                if (i >= line.length()) break;

                size_t start = i;
                if (line[i] == '"') { // Quoted path
                    start++; // Skip the opening quote
                    i++;
                    while (i < line.length() && line[i] != '"') {
                        i++;
                    }
                } else { // Unquoted path
                    while (i < line.length() && !isspace(line[i])) {
                        i++;
                    }
                }
                config.input_files.push_back(line.substr(start, i - start));
                if (i < line.length() && line[i] == '"') i++; // Skip closing quote for next iteration
            }
            if (config.input_files.size() >= MAX_FILES) {
                std::cout << "Maximum number of files (" << MAX_FILES << ") reached." << std::endl;
            }
        }
        if (config.input_files.empty()) {
            std::cout << "No input files were entered. Exiting." << std::endl;
            return 1;
        }
    }

    get_other_config(&config, &seed);
    run_yolo_process(&config, seed);
    return 0;
}
