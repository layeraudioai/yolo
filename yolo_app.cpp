#include "yolo_core.hpp"

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

        // Create a vector of input channel indices to be shuffled for each output channel.
        std::vector<int> input_indices(num_input_channels);
        for(int k=0; k<num_input_channels; ++k) input_indices[k] = k;

        // Define each output channel (c0, c1, ... c<num_output_channels-1>)
        for (int i = 0; i < num_output_channels; ++i) {
            pan_str += "|c" + std::to_string(i) + "=";
            bool first_mix = true;

            // std::shuffle requires a random number generator. We'll create one seeded from the C-style rand().
            std::mt19937 g(rand());
            std::shuffle(input_indices.begin(), input_indices.end(), g);

            // Mix from the available input channels in a random order.
            for (int input_channel_index : input_indices) {
                if (rand() % 2 == 0) { // Randomly decide to mix from channel j
                    if (!first_mix) pan_str += (rand() % 2 == 0) ? "+" : "-"; // Randomly add or subtract
                    pan_str += "c" + std::to_string(input_channel_index);
                    first_mix = false;
                }
            }
            // Ensure the channel definition is not empty, falling back to a default mix.
            if (first_mix) pan_str += "c" + std::to_string(i % num_input_channels); // e.g., c0=c0, c1=c1
        }
    }
    return pan_str;
}

// Special exception to signal a graceful exit request from a prompt.
class UserExitException : public std::exception {
public:
    const char* what() const noexcept override {
        return "User requested exit.";
    }
};

// Template for reading any type that supports `std::cin >>`
template<typename T>
void prompt_for_value(const std::string& prompt, T& value) {
    std::string line;
    while (true) {
        std::cout << prompt;
        std::getline(std::cin, line);

        if (line == "exit") {
            throw UserExitException();
        }

        // Use a stringstream to attempt conversion from the read line.
        std::stringstream ss(line);
        ss >> value;

        // Check if the conversion was successful and the entire string was consumed.
        if (ss.good() || ss.eof()) {
            break;
        }

        std::cout << "Invalid input. Please try again.\n";
    }
}

// Overload for std::string to correctly read entire lines, including spaces.
void prompt_for_value(const std::string& prompt, std::string& value) {
    std::cout << prompt;
    std::getline(std::cin, value);
    if (value == "exit") {
        throw UserExitException();
    }
};

void get_other_config(YoloConfig *config, int *seed) {
    try {
        if (config->layer_files=='-') {
            prompt_for_value("Generate layered files? (y/n) (or 'exit'): ", config->layer_files);
        }
        if (config->create_hyper_file=='-') {
            prompt_for_value("Create a hyper file? (y/n) (or 'exit'): ", config->create_hyper_file);
        }
        if (config->create_hyper_file=='y') {
            prompt_for_value("Hyper file name? (or 'exit'): ", config->hyper_file_name);
        }
        if (config->remix_enabled=='-') {
            prompt_for_value("Enable remixing (sample rearrangement)? (y/n) (or 'exit'): ", config->remix_enabled);
        }
        if (config->remix_enabled=='y') {
            if (config->remix_seed == 0)
                prompt_for_value("Enter remix seed (0 for random) (or 'exit'): ", config->remix_seed);
            else if (config->remix_seed == -1) // Use -1 to signify 'not set by user'
                prompt_for_value("Enter remix seed (0 for random) (or 'exit'): ", config->remix_seed);
            if (config->remix_intensity == -1.0f)
                prompt_for_value("Enter remix intensity (0.05-100) (or 'exit'): ", config->remix_intensity);
        }
        std::cout << "Audio options:\n";
        if (config->bass_boost == -1.0f)
            prompt_for_value("Enter bass boost (or 'exit'): ", config->bass_boost);
        if (config->treble_gain == -1.0f)
            prompt_for_value("Enter treble gain (or 'exit'): ", config->treble_gain);
        if (config->volume_lufs == -1.0f)
            prompt_for_value("Enter volume LUFS (or 'exit'): ", config->volume_lufs);
        if (config->tempo_modifier == -1.0f)
            prompt_for_value("Enter tempo modifier (or 'exit'): ", config->tempo_modifier);
        if (config->quality == -1)
            prompt_for_value("Enter quality (0[max]-31[lowest]) (or 'exit'): ", config->quality);
        if (config->num_audio_channels == -1)
            prompt_for_value("Enter number of output audio channels (or 'exit'): ", config->num_audio_channels);
        // Prompt for audio extension only if it wasn't set via command-line argument.
        if (config->audio_output_extension_is_default)
            prompt_for_value("Enter audio output extension (e.g., mp3, ogg) (or 'exit'): ", config->audio_output_extension);

        // Prompt for MIDI/SF2 if not provided
        // Only prompt for MIDI/SF2 if MIDI hasn't been explicitly disabled.
        if (config->midi_path != "disabled") {
            if (config->midi_path.empty()) {
                prompt_for_value("Enter path to MIDI file (or leave blank, or 'exit'): ", config->midi_path);
            }
            // If a MIDI file is now present (either from CLI or prompt), and no SF2 is set, prompt for SF2.
            if (!config->midi_path.empty() && config->sf2_path.empty())
                prompt_for_value("Enter path to SF2 SoundFont (or 'exit'): ", config->sf2_path);
        }
    
        // Check if there are any video files before asking for video-specific options.
        bool has_video_files = false;
        for (const auto& file : config->input_files) {
            if (has_video_stream(file, config, nullptr)) { // log_file is not available here
                has_video_files = true;
                break;
            }
        }
        if (has_video_files) {
            std::cout << "Video specific options:\n";
            if (config->brightnessTarget == -1.0f)
                prompt_for_value("Enter brightness target (or 'exit'): ", config->brightnessTarget);
            if (config->contrastTarget == -1.0f)
                prompt_for_value("Enter contrast target (or 'exit'): ", config->contrastTarget);
            if (config->saturationTarget == -1.0f)
                prompt_for_value("Enter saturation target (or 'exit'): ", config->saturationTarget);
            if (config->video_output_extension == "mkv") // Not set by user arg
                prompt_for_value("Enter video output extension (e.g., mkv, mp4) (or 'exit'): ", config->video_output_extension);
            if (config->video_res.empty())
                prompt_for_value("Enter video output resolution (e.g., 1920x1080) (or 'exit'): ", config->video_res);
            if (config->video_fps.empty())
                prompt_for_value("Enter video output framerate (e.g., 30) (or 'exit'): ", config->video_fps);
        }
        std::cout << "General options:\n";
        if (config->output_dir.empty()) {
            prompt_for_value("Enter output directory (leave blank for current, or 'exit'): ", config->output_dir);
            // If the user leaves it blank, explicitly set it to "."
            if (config->output_dir.empty()) {
                config->output_dir = ".";
            }
        }

        if (config->num_runs == -1)
            prompt_for_value("Enter number of runs (or 'exit'): ", config->num_runs);
        if (*seed == 0) 
            prompt_for_value("Enter random seed (0 for random) (or 'exit'): ", *seed);
        if (config->runNumber_is_default && *seed != 0) // Only prompt if not set by arg and a seed is used
            prompt_for_value("Starting run number (or 'exit'): ", config->runNumber);

    } catch (const UserExitException&) {
        // Re-throw to be caught by main
        throw;
    }
}

/**
 * @brief Trims leading and trailing whitespace and removes quotes from a string.
 *
 * @param s The string to trim.
 */
void trim_and_unquote(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    if (s.length() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.length() - 2);
    }
}

void log_current_time(FILE *f) {
    if (!f) return; // Prevent crash if log file is not open
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

/**
 * @brief Checks if a media file contains a video stream using ffprobe. Caches results.
 *
 * This is more reliable than checking extensions, as it correctly identifies
 * audio files with embedded cover art as having a video stream.
 *
 * @param filename The path to the media file.
 * @param config Pointer to the YoloConfig object for caching.
 * @param log_file Optional file pointer for logging. Can be nullptr.
 * @return true if a video stream is detected, false otherwise.
 */
bool has_video_stream(const std::string& filename, YoloConfig* config, FILE* log_file) {
    // Check cache first
    auto it = config->video_stream_cache.find(filename);
    if (it != config->video_stream_cache.end()) {
        return it->second;
    }

    std::string command = std::string(FFPROBE_PATH) + " -v error -select_streams v:0 -show_entries stream=codec_type -of default=noprint_wrappers=1:nokey=1 \"" + filename + "\"";
    std::string output;
    bool result = false;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        if (log_file) fprintf(log_file, "  ERROR: popen failed for video stream detection on '%s'.\n", filename.c_str());
        // Cache and return false on error to avoid retrying
        config->video_stream_cache[filename] = false;
        return false;
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        output = buffer;
        // ffprobe will output "video\n" if a video stream is found.
        if (output.find("video") != std::string::npos) {
            result = true;
        }
    }
    pclose(pipe);

    // Store result in cache
    config->video_stream_cache[filename] = result;
    return result;
}

/**
 * @brief Joins a directory path and a filename, ensuring correct path separators.
 *
 * @param dir The directory path. Can be empty.
 * @param filename The name of the file.
 * @return The combined path.
 */
#include <algorithm> // Needed for std::replace

std::string join_path(const std::string& dir, const std::string& filename) {
    if (dir.empty()) {
        return filename;
    }
    
    std::string f = filename;
    // Remove leading separator if present, so it's not treated as absolute
    if (!f.empty() && (f[0] == '/' || f[0] == '\\')) {
        f = f.substr(1);
    }
    
    std::filesystem::path p(dir);
    p /= f;
    
    std::string res = p.string();
#ifdef _WIN32
    std::replace(res.begin(), res.end(), '/', '\\');
#endif
    return res;
}

/**
 * @brief Ensures that the specified directory exists.
 *
 * @param path The directory path to check and create if necessary.
 */
void ensure_directory_exists(const std::string& path) {
    if (path.empty()) {
        return;
    }
    // This is a simple implementation. For nested directories, a more robust
    // solution (like std::filesystem::create_directories in C++17) would be needed.
    MKDIR(path.c_str());
}

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
    // Construct command with extra quotes for safety
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

    // Construct the command string properly for CreateProcessA
    std::string full_command = std::string(FFPROBE_PATH) + " -v error -select_streams a:0 -show_entries stream=channels -of default=noprint_wrappers=1:nokey=1 \"" + filename + "\"";
    
    // Log the command and working directory
    FILE* debug_log = fopen("debug_probe.log", "a");
    fprintf(debug_log, "Probing file: %s, Exists: %s\n", filename.c_str(), (_access(filename.c_str(), 0) == 0) ? "yes" : "no");
    fprintf(debug_log, "Full command: %s\n", full_command.c_str());
    fclose(debug_log);

    std::vector<char> cmd_line(full_command.begin(), full_command.end());
    cmd_line.push_back('\0');

    if (!CreateProcessA(NULL, cmd_line.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(log_file, "  ffprobe CreateProcess failed (%ld). Command: %s\n", GetLastError(), full_command.c_str());
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
        } else if (log_file) { fprintf(log_file, "  ffprobe returned no output for channel count on '%s'.\n", filename.c_str()); }
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

// Use ffmpeg's ametadata filter to detect audio onsets (transients)
std::vector<double> get_audio_onsets(const std::string& input_file, FILE* log_file) {
    std::vector<double> onsets;
    // Command to run ffmpeg, analyze with ametadata, and print onset times to stderr.
    // We use a null output to prevent creating a file.
    std::string command = std::string(FFMPEG_PATH) + " -i \"" + input_file + "\" -af \"ametadata=mode=print:key=lavfi.onset.time\" -f null -";

    fprintf(log_file, "  Detecting onsets for '%s'\n", input_file.c_str());

    // We need to capture stderr, not stdout.
#ifdef _WIN32
    command += " 2>&1"; // Redirect stderr to stdout on Windows
#else
    command += " 2>&1"; // Works for POSIX too
#endif

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        fprintf(log_file, "  ERROR: popen failed for onset detection.\n");
        return onsets;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        std::string line(buffer);
        size_t pos = line.find("lavfi.onset.time:");
        if (pos != std::string::npos) {
            double onset_time = std::stod(line.substr(pos + 17));
            onsets.push_back(onset_time);
        }
    }
    pclose(pipe);
    fprintf(log_file, "  Detected %zu onsets.\n", onsets.size());
    return onsets;
}
// Shuffle an audio file by decoding to raw PCM, shuffling chunks, and writing to a temp WAV file.
struct ShuffledMedia {
    std::string audio_file;
    std::string video_file;
    bool success = false;
};

ShuffledMedia shuffle_media_file(const std::string& input_file, const std::string& temp_audio_output, const std::string& temp_video_output, unsigned int remix_seed, float remix_intensity, FILE* log_file) {
    ShuffledMedia result;
    log_current_time(log_file);
    fprintf(log_file, "  Shuffling '%s' with seed %u, intensity %.2f\n", input_file.c_str(), remix_seed, remix_intensity);

    // 1. Get media properties using separate, targeted ffprobe commands to avoid ambiguity.
    int sample_rate = 44100;
    int channels = 2;
    int width = 0, height = 0;
    float frame_rate = 0.0f;
    std::vector<std::string> temp_files_to_delete;
    // --- Probe for Audio Properties ---
    std::string audio_probe_cmd = std::string(FFPROBE_PATH) + " -v error -select_streams a:0 -show_entries stream=sample_rate,channels -of default=noprint_wrappers=1:nokey=1 \"" + input_file + "\"";
    FILE* pipe = popen(audio_probe_cmd.c_str(), "r");
    if (!pipe) {
        fprintf(log_file, "  ERROR: ffprobe popen failed for audio properties.\n");
        return result;
    }
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) { sample_rate = atoi(buffer); }
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) { channels = atoi(buffer); }
    pclose(pipe);

    // --- Probe for Video Properties (if a video stream exists) ---
    // We must pass a YoloConfig object to has_video_stream for caching.
    // Since we don't have the main one, we create a temporary one. This is acceptable
    // as its cache will just be discarded after this function.
    YoloConfig temp_config_for_probe;
    bool has_video = has_video_stream(input_file, &temp_config_for_probe, log_file);

    if (has_video) {
        std::string video_probe_cmd = std::string(FFPROBE_PATH) + " -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate -of default=noprint_wrappers=1:nokey=1 \"" + input_file + "\"";
        pipe = popen(video_probe_cmd.c_str(), "r");
        if (pipe) {
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) { width = atoi(buffer); }
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) { height = atoi(buffer); }
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) { frame_rate = std::stof(buffer); }
            pclose(pipe);
        }
    }

    if (sample_rate <= 0 || channels <= 0) {
        fprintf(log_file, "  ERROR: Failed to get valid sample rate/channels for shuffling.\n");
        return result;
    }

    // --- Audio Decoding ---
    std::string decode_cmd = std::string(FFMPEG_PATH) + " -i \"" + input_file + "\" -f f32le -ac " + std::to_string(channels) + " -ar " + std::to_string(sample_rate) + " -";

    pipe = popen(decode_cmd.c_str(), "rb"); // Use "rb" for binary read on Windows
    if (!pipe) {
        fprintf(log_file, "  ERROR: ffmpeg popen failed for shuffle decode.\n");
        return result;
    }

    // 2. Read all raw PCM data into a vector.
    std::vector<char> pcm_data;
    char read_buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(read_buffer, 1, sizeof(read_buffer), pipe)) > 0) {
        pcm_data.insert(pcm_data.end(), read_buffer, read_buffer + bytes_read);
    }
    pclose(pipe);

    // 3. Get rhythmic onsets to create musically-aware chunks.
    std::vector<double> onsets = get_audio_onsets(input_file, log_file);

    const size_t bytes_per_sample = 4; // f32le
    const size_t bytes_per_second = sample_rate * channels * bytes_per_sample;

    if (pcm_data.empty()) {
        fprintf(log_file, "  ERROR: No PCM data for shuffling.\n");
        return result;
    }

    // A "chunk" is a region of data between onsets. We'll store audio and video chunks separately.
    std::vector<std::vector<char>> audio_chunks;

    if (onsets.size() > 1) {
        // Rhythmic chunking based on detected onsets
        const size_t frame_size_bytes = channels * bytes_per_sample;
        if (frame_size_bytes == 0) return result; // Avoid division by zero

        size_t last_pos = 0;
        for (size_t i = 0; i < onsets.size(); ++i) {
            size_t current_pos = static_cast<size_t>(onsets[i] * bytes_per_second);
            // Align the cut to the start of an audio frame to prevent corruption.
            current_pos -= (current_pos % frame_size_bytes);

            if (current_pos > last_pos) {
                audio_chunks.emplace_back(pcm_data.begin() + last_pos, pcm_data.begin() + current_pos);
            }
            last_pos = current_pos;
        }
        // Add the final part of the audio after the last onset
        if (last_pos < pcm_data.size()) {
            audio_chunks.emplace_back(pcm_data.begin() + last_pos, pcm_data.end());
        }
    } else {
        // Fallback to fixed-time chunking if no onsets were found
        fprintf(log_file, "  No onsets found, falling back to fixed-duration chunking.\n");
        const float chunk_duration_seconds = 0.2f; // A slightly larger default
        const size_t chunk_size_bytes = static_cast<size_t>(chunk_duration_seconds * bytes_per_second);
        if (chunk_size_bytes == 0) return result;
        for (size_t i = 0; i < pcm_data.size(); i += chunk_size_bytes) {
            size_t end = std::min(i + chunk_size_bytes, pcm_data.size());
            audio_chunks.emplace_back(pcm_data.begin() + i, pcm_data.begin() + end);
        }
    }

    // Use remix_intensity to determine how many chunks to shuffle.
    // Clamp intensity to the valid range [0.05, 100.0].
    float intensity = std::max(0.05f, std::min(100.0f, remix_intensity));
    size_t num_chunks_to_shuffle = static_cast<size_t>(audio_chunks.size() * (intensity / 100.0f));

    // To shuffle only a subset, we create a vector of indices, shuffle it,
    // and then pick the first 'num_chunks_to_shuffle' indices to actually move.
    std::vector<size_t> indices(audio_chunks.size());
    for(size_t i = 0; i < audio_chunks.size(); ++i) indices[i] = i;

    std::mt19937 g(remix_seed);
    std::shuffle(indices.begin(), indices.end(), g);

    // Create a new vector for the final chunk order.
    std::vector<std::vector<char>> final_audio_chunks = audio_chunks;
    for (size_t i = 0; i < num_chunks_to_shuffle; ++i) {
        final_audio_chunks[indices[i]] = audio_chunks[i];
    }    

    // 4. Process and write the shuffled (and now transformed) audio chunks to a new temporary WAV file.
    // We will pipe each chunk to a separate ffmpeg process for transformation.
    // The final output will be a concatenation of these processed chunks.    
    std::string final_concat_list_path = "temp_concat_list_" + std::to_string(remix_seed) + ".txt";
    FILE* concat_list_file = fopen(final_concat_list_path.c_str(), "w");
    if (!concat_list_file) {
        fprintf(log_file, "  ERROR: Could not create temporary concat list file.\n");
        return result;
    }

    temp_files_to_delete.push_back(final_concat_list_path); // Make sure we clean this up

    fprintf(log_file, "  Applying transformations to audio chunks...\n");
    for (const auto& chunk : final_audio_chunks) {
        std::string temp_chunk_file = "temp_chunk_" + std::to_string(remix_seed) + "_" + std::to_string(rand()) + ".wav";
        temp_files_to_delete.push_back(temp_chunk_file); // And this

        // Decide if we should apply a filter to this chunk.
        // The chance increases with intensity.
        float transform_chance = intensity / 100.0f;
        std::string filter_chain;

        if (((float)rand() / RAND_MAX) < transform_chance) {
            int choice = rand() % 5;
            if (choice == 0) { // Pitch Shift
                float semitones[] = {-12.0f, -7.0f, -5.0f, 5.0f, 7.0f, 12.0f};
                float pitch_shift = semitones[rand() % 6];
                filter_chain = "asetrate=" + std::to_string(sample_rate * pow(2.0, pitch_shift / 12.0)) + ",atempo=" + std::to_string(1.0 / pow(2.0, pitch_shift / 12.0));
            } else if (choice == 1) { // Reverse
                filter_chain = "areverse";
            } else if (choice == 2) { // Vibrato/Tremolo
                filter_chain = (rand() % 2 == 0) ? "vibrato=f=5.0:d=0.5" : "tremolo=f=10.0:d=0.7";
            } else if (choice == 3) { // Add synthetic tone
                float freq = 220.0f * pow(2.0, (rand() % 24) / 12.0); // Random note in a 2-octave range
                filter_chain = "asplit[main][ov];[ov]sine=f=" + std::to_string(freq) + ":d=999,volume=0.2[tone];[main][tone]amix";
            } else { // Time stretch
                filter_chain = (rand() % 2 == 0) ? "atempo=2.0" : "atempo=0.5";
            }
        }

        std::string process_chunk_cmd = std::string(FFMPEG_PATH) + " -y -f f32le -ar " + std::to_string(sample_rate) + " -ac " + std::to_string(channels) + " -i - ";
        if (!filter_chain.empty()) {
            process_chunk_cmd += "-af \"" + filter_chain + "\" ";
        }
        process_chunk_cmd += "-c:a pcm_s16le \"" + temp_chunk_file + "\"";

        pipe = popen(process_chunk_cmd.c_str(), "wb");
        if (pipe) {
            fwrite(chunk.data(), 1, chunk.size(), pipe);
            pclose(pipe);
            fprintf(concat_list_file, "file '%s'\n", temp_chunk_file.c_str());
        }
    }
    fclose(concat_list_file);

    // 5. Concatenate all the processed temporary chunk files into the final shuffled audio file.
    std::string concat_cmd = std::string(FFMPEG_PATH) + " -y -f concat -safe 0 -i \"" + final_concat_list_path + "\" -c copy \"" + temp_audio_output + "\"";
    log_current_time(log_file);
    fprintf(log_file, "  Concatenating transformed chunks into '%s'\n", temp_audio_output.c_str());
    system(concat_cmd.c_str());

    pclose(pipe);
    result.audio_file = temp_audio_output;

    // --- Video Shuffling (if video is present) ---
    if (has_video) {
        fprintf(log_file, "  Shuffling video stream...\n");
        const size_t frame_size = width * height * 3 / 2; // For yuv420p
        std::string decode_video_cmd = std::string(FFMPEG_PATH) + " -i \"" + input_file + "\" -f rawvideo -pix_fmt yuv420p -";
        
        pipe = popen(decode_video_cmd.c_str(), "rb");
        if (!pipe) {
            fprintf(log_file, "  ERROR: ffmpeg popen failed for video decode.\n");
            return result;
        }

        std::vector<char> video_data;
        while ((bytes_read = fread(read_buffer, 1, sizeof(read_buffer), pipe)) > 0) {
            video_data.insert(video_data.end(), read_buffer, read_buffer + bytes_read);
        }
        pclose(pipe);

        // Slice video data into chunks corresponding to audio chunks
        std::vector<std::vector<char>> video_chunks;
        size_t total_frames_processed = 0;
        for (const auto& audio_chunk : audio_chunks) {
            double audio_chunk_duration = (double)audio_chunk.size() / bytes_per_second;
            size_t num_frames_in_chunk = static_cast<size_t>(audio_chunk_duration * frame_rate);
            
            size_t start_byte = total_frames_processed * frame_size;
            size_t end_byte = start_byte + (num_frames_in_chunk * frame_size);

            if (end_byte > video_data.size()) end_byte = video_data.size();

            if (start_byte < end_byte) {
                video_chunks.emplace_back(video_data.begin() + start_byte, video_data.begin() + end_byte);
            }
            total_frames_processed += num_frames_in_chunk;
        }

        // Shuffle video chunks using the *same* shuffled index map as the audio
        std::vector<std::vector<char>> final_video_chunks = video_chunks;
        if (video_chunks.size() == audio_chunks.size()) {
            for (size_t i = 0; i < num_chunks_to_shuffle; ++i) {
                final_video_chunks[indices[i]] = video_chunks[i];
            }
        }

        // Re-encode the shuffled video frames into a lossless temporary file
        std::string encode_video_cmd = std::string(FFMPEG_PATH) + " -y -f rawvideo -pix_fmt yuv420p -s " + std::to_string(width) + "x" + std::to_string(height) + " -r " + std::to_string(frame_rate) + " -i - -c:v ffv1 \"" + temp_video_output + "\"";
        pipe = popen(encode_video_cmd.c_str(), "wb");
        if (!pipe) {
            fprintf(log_file, "  ERROR: ffmpeg popen failed for video encode.\n");
            return result;
        }
        for (const auto& chunk : final_video_chunks) {
            fwrite(chunk.data(), 1, chunk.size(), pipe);
        }
        pclose(pipe);
        result.video_file = temp_video_output;
    }

    result.success = true;
    return result;
}

void run_yolo_process(YoloConfig *config, int seed) {
    std::vector<std::string> temp_files_to_delete;

    // Ensure the output directory exists before proceeding.
    ensure_directory_exists(config->output_dir);

    std::string log_path = join_path(config->output_dir, "yolo.log");
    FILE *log_file = fopen(log_path.c_str(), "a");
    if (!log_file) {
        fprintf(stderr, "Failed to open log file '%s' for writing.\n", log_path.c_str());
        return;
    }

    fprintf(log_file, "--- Yolo Process Started (Seed: %d) ---\n", seed);
    fprintf(log_file, "Configuration:\n");
    fprintf(log_file, "  Number of Runs: %d\n", config->num_runs);
    fprintf(log_file, "----------------------------\n");

    ProcessHandle processes[MAX_PROCESSES];
    int process_count = 0;

    for (int i = 0; i < config->num_runs; i++) {
        int run = config->runNumber + i;
        // Re-seed the RNG for each run to get different random values.
        // The 'mixing' seed controls effects like pan, volume, EQ, etc.
        // Using seed + run makes the results for each run deterministic and reproducible.
        unsigned int current_seed = (seed == 0) ? (unsigned int)time(NULL) + run : seed + run;
        srand(current_seed);

        // The 'remixing' seed controls the ashuffle filter for sample rearrangement.
        unsigned int current_remix_seed = (config->remix_seed == 0) ? (unsigned int)time(NULL) + run : config->remix_seed + run;

        // Recalculate randomized parameters for each run
        // Use a more robust random number generator for floats
        std::mt19937 rng(current_seed);
        std::uniform_real_distribution<float> dist_wide(0.0f, 1.0f);
        std::uniform_real_distribution<float> dist_narrow(0.0f, 1.0f);

        config->brightness = config->brightnessTarget + (dist_narrow(rng) * 0.5f) - 0.25f;
        config->contrast = config->contrastTarget + (dist_narrow(rng) * 0.5f) - 0.25f;
        config->saturation = config->saturationTarget + (dist_narrow(rng) * 0.5f) - 0.25f;
        config->volume = config->volume_lufs + (dist_narrow(rng) * 0.6f) + 0.25f;
        config->treble = config->treble_gain + (dist_narrow(rng) * 0.5f) - 0.25f;
        config->bass = config->bass_boost + (dist_wide(rng) * 0.8f) - 0.25f;
        config->atempo = config->tempo_modifier + (dist_narrow(rng) * 0.5f) - 0.25f;
        if (config->atempo <= 0) config->atempo = 0.5; // Prevent invalid tempo values
        config->vtempo = 1.0 / config->atempo;

        log_current_time(log_file); 
        fprintf(log_file, "Starting Run %d with Mix Seed %u, Remix Seed %u\n", run, current_seed, (config->remix_enabled == 'y' ? current_remix_seed : 0)); fflush(log_file);

        std::vector<std::string> current_run_inputs = config->input_files; // Start with original inputs
        config->brightness = config->brightnessTarget + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->contrast = config->contrastTarget + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->saturation = config->saturationTarget + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->volume = config->volume_lufs + ((float)rand()/(float)RAND_MAX * 0.6) + 0.25;
        config->treble = config->treble_gain + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->bass = config->bass_boost + ((float)rand()/(float)RAND_MAX * 0.8) - 0.25;
        config->atempo = config->tempo_modifier + ((float)rand()/(float)RAND_MAX * 0.5) - 0.25;
        config->vtempo = 1.0 / config->atempo;

        // --- MIDI Synthesis (if enabled) ---
        if (!config->midi_path.empty() && !config->sf2_path.empty()) {
            std::string synth_output_file = join_path(config->output_dir, "temp_synth_audio_" + std::to_string(run) + ".wav");
            std::string synth_cmd = std::string(FLUIDSYNTH_PATH) + " -ni -F \"" + synth_output_file + "\" -r 44100 \"" + config->sf2_path + "\" \"" + config->midi_path + "\"";
            
            log_current_time(log_file);
            fprintf(log_file, "Run %d: Synthesizing MIDI with SoundFont...\n", run);
            fprintf(log_file, "  Command: %s\n", synth_cmd.c_str());
            fflush(log_file);

            int synth_status = system(synth_cmd.c_str());
            if (synth_status == 0) {
                current_run_inputs.push_back(synth_output_file);
                temp_files_to_delete.push_back(synth_output_file);
                fprintf(log_file, "  MIDI synthesis successful. Added '%s' to inputs.\n", synth_output_file.c_str());
            } else {
                fprintf(log_file, "  WARNING: fluidsynth command failed with status %d. MIDI will not be included.\n", synth_status);
            }
        }

        if (config->layer_files=='y') {
            // --- Layered files logic ---
            // A single ffmpeg command per run, with all files as inputs.
            bool has_video_input = false;
            for (size_t file_idx = 0; file_idx < current_run_inputs.size(); ++file_idx) {
                if (has_video_stream(current_run_inputs[file_idx], config, log_file)) {
                    has_video_input = true;
                    break;
                }
            }

            // Determine output filename and extension
            const std::string& output_ext = has_video_input ? config->video_output_extension : config->audio_output_extension;
            char output_filename[512];
            snprintf(output_filename, sizeof(output_filename), "%s", join_path(config->output_dir, "layered_output_run" + std::to_string(run) + "." + output_ext).c_str());

            // Build input string: -i "file1" -i "file2" ...
            std::string input_str;
            if (config->remix_enabled == 'y') { // Remixing original files before layering
                for (size_t file_idx = 0; file_idx < config->input_files.size(); ++file_idx) { // Only iterate original files
                    std::string temp_audio_name = join_path(config->output_dir, "temp_remix_audio_" + std::to_string(run) + "_" + std::to_string(file_idx) + ".wav");
                    std::string temp_video_name = join_path(config->output_dir, "temp_remix_video_" + std::to_string(run) + "_" + std::to_string(file_idx) + ".mkv");
                    
                    ShuffledMedia media = shuffle_media_file(config->input_files[file_idx], temp_audio_name, temp_video_name, current_remix_seed + file_idx, config->remix_intensity, log_file);
                    if (media.success) {
                        current_run_inputs[file_idx] = media.audio_file; // The primary input is now the shuffled audio
                        temp_files_to_delete.push_back(media.audio_file);
                        if (!media.video_file.empty()) {
                            // If video was shuffled, we need to add it as a separate input
                            input_str += "-i \"" + media.video_file + "\" ";
                            temp_files_to_delete.push_back(media.video_file);
                        }
                    }
                }
            }

            for (const auto& file : current_run_inputs) {
                input_str += "-i \"" + file + "\" ";
            }

            // Build filter_complex string
            std::stringstream filter_complex;
            std::vector<int> audio_stream_indices;
            std::vector<int> video_stream_indices;

            // Helper to check for audio stream (more reliable than just extension)
            auto has_audio_stream = [&](const std::string& file) {
                // A simple proxy for now: if channel count > 0, it has audio.
                // This leverages the existing ffprobe call.
                return get_audio_channel_count(file, log_file) > 0;
            };

            for (size_t i = 0; i < current_run_inputs.size(); ++i) {
                if (has_video_stream(current_run_inputs[i], config, log_file)) {
                    video_stream_indices.push_back(i);
                }
                // Only add to audio processing if it actually has an audio stream
                if (has_audio_stream(current_run_inputs[i])) {
                    audio_stream_indices.push_back(i);
                }
            }

            // --- Correctly determine total input channels ---
            int total_input_channels = 0;
            log_current_time(log_file);
            fprintf(log_file, "Run %d: Detecting input channel counts for layering...\n", run);
            for (const auto& file : current_run_inputs) {
                // We get channel count again, but it should be cached by ffprobe calls.
                int file_channels = get_audio_channel_count(file, log_file); 
                fprintf(log_file, "  - '%s': %d channels\n", file.c_str(), file_channels);
                total_input_channels += file_channels;
            }
            fprintf(log_file, "  Total input channels for pan filter: %d\n", total_input_channels);
            fflush(log_file);

            // Audio chain
            if (!audio_stream_indices.empty()) {
                for (int index : audio_stream_indices) {
                    // Apply tempo modification to each audio input *before* merging
                    // This is crucial for keeping remixed and synthesized audio in sync
                    filter_complex << "[" << index << ":a]atempo=" << config->atempo << "[a" << index << "];";
                }
                for (int index : audio_stream_indices) {
                    filter_complex << "[a" << index << "]";
                }
                filter_complex << "amerge=inputs=" << audio_stream_indices.size() << "[a_merged];";
                
                std::string pan_filter = get_pan_filter_string(total_input_channels, config->num_audio_channels);
                // Tempo is already applied, so we don't apply it again to the merged stream
                filter_complex << "[a_merged]volume=" << config->volume
                               << ",bass=gain=" << config->bass
                               << ",treble=gain=" << config->treble
                               << "," << pan_filter << "[a_out];";
            } else {
                // No audio inputs, so no audio chain.
            }

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
            
            // Add video options if resolution or framerate are specified
            if (!config->video_res.empty()) {
                cmd_str += " -s " + config->video_res;
            }
            if (!config->video_fps.empty()) {
                cmd_str += " -r " + config->video_fps;
            }

            if (!video_stream_indices.empty()) {
                cmd_str += " -map \"[v_out]\" -q:v " + std::to_string(config->quality);
            }

            if (!audio_stream_indices.empty()) {
                cmd_str += " -map \"[a_out]\"";
                // Handle audio options, especially the MP3 channel constraint.
                if (config->audio_output_extension == "mp3") {
                    // MP3 must be 2 channels, regardless of what the pan filter did.
                    cmd_str += " -q:a " + std::to_string(config->quality) + " -ac 2";
                } else {
                    // For other formats, use the user-specified channel count.
                    cmd_str += " -q:a " + std::to_string(config->quality) + " -ac " + std::to_string(config->num_audio_channels);
                }
            } else {
                // If there are no audio inputs, we must add -an to prevent ffmpeg from failing
                cmd_str += " -an";
            }

            cmd_str += " -y \"";
            cmd_str += output_filename;
            cmd_str += "\"";
            if (config->create_hyper_file == 'y' && !config->hyper_file_name.empty()) {
                cmd_str += " \"" +config->hyper_file_name + "\"";
            } 

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
            for (size_t file_idx = 0; file_idx < config->input_files.size(); file_idx++) {
                std::string current_input_file = config->input_files[file_idx];

                if (config->remix_enabled == 'y') {
                    std::string temp_audio_name = join_path(config->output_dir, "temp_remix_audio_" + std::to_string(run) + "_" + std::to_string(file_idx) + ".wav");
                    std::string temp_video_name = join_path(config->output_dir, "temp_remix_video_" + std::to_string(run) + "_" + std::to_string(file_idx) + ".mkv");
                    
                    ShuffledMedia media = shuffle_media_file(current_input_file, temp_audio_name, temp_video_name, current_remix_seed + file_idx, config->remix_intensity, log_file);
                    if (media.success) {
                        current_input_file = media.audio_file;
                        temp_files_to_delete.push_back(media.audio_file);
                        // In per-file mode, if video was shuffled, we need to replace the video source as well.
                        // This is tricky. The simplest way is to use the shuffled video as a second input and map it.
                        // For now, we'll focus on the audio part which is what current_input_file tracks.
                    }
                }

                bool is_video = has_video_stream(config->input_files[file_idx], config, log_file); // Check original for video properties
                const std::string& output_ext = is_video ? config->video_output_extension : config->audio_output_extension;
                std::string base_filename = get_basename(config->input_files[file_idx]);
                char output_filename[512];
                std::string final_output_name = base_filename + "_run" + std::to_string(run) + "_file" + std::to_string(file_idx) + "." + output_ext;
                // Use %zu for size_t type 'i' to fix format mismatch warning/error.
                snprintf(output_filename, sizeof(output_filename), "%s",
                         join_path(config->output_dir, final_output_name).c_str());

                char filter_complex_a[4096];
                char filter_complex_v[512];
                // Correctly get the channel count of the *input* file for the pan filter.
                int input_channels = get_audio_channel_count(current_input_file, log_file);

                // Build the audio filter chain string.
                std::string audio_filter_chain;
                // ashuffle is no longer needed here as it's done pre-process

                std::string pan_filter = get_pan_filter_string(input_channels, config->num_audio_channels);
                snprintf(filter_complex_a, sizeof(filter_complex_a),
                    "%satempo=%.6f,volume=%.6f,bass=gain=%.6f,treble=gain=%.6f,%s", // pan_filter is safe
                    audio_filter_chain.c_str(), // This is now correctly matched with the leading %s
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

                    if (config->create_hyper_file=='y') { 
                        std::cout << "Using: " << config->hyper_file_name << std::endl;
                        cmd_str = std::string(FFMPEG_PATH) + " -i \"" + current_input_file + "\"" +
                              " -filter_complex:a \"" + std::string(filter_complex_a) + "\"" +
                              " -filter_complex:v \"" + std::string(filter_complex_v) + "\"";
                              cmd_str += " -q:v " + std::to_string(config->quality) +
                              // Add video options if resolution or framerate are specified
                              (!config->video_res.empty() ? " -s " + config->video_res : "") +
                              (!config->video_fps.empty() ? " -r " + config->video_fps : "") +
                              audio_codec_str +
                              " -ac " + std::to_string(config->num_audio_channels) + " -y \"" + output_filename + "\"" + 
                              " -y \"" + output_filename + "\" \"" + config->hyper_file_name + "\""; }
                    else { 
                        std::cout << "Using: " << config->hyper_file_name << std::endl;
                        cmd_str = std::string(FFMPEG_PATH) + " -i \"" + current_input_file + "\"" +
                              " -filter_complex:a \"" + std::string(filter_complex_a) + "\"" +
                              " -filter_complex:v \"" + std::string(filter_complex_v) + "\"";
                              cmd_str += " -q:v " + std::to_string(config->quality) +
                              // Add video options if resolution or framerate are specified
                              (!config->video_res.empty() ? " -s " + config->video_res : "") +
                              (!config->video_fps.empty() ? " -r " + config->video_fps : "") +
                              audio_codec_str +
                              " -ac " + std::to_string(config->num_audio_channels) + " -y \"" + output_filename + "\"" +
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
                              current_input_file + "\"" + " -af \"" +
                              std::string(filter_complex_a) + "\"" + audio_opts_str + " -y \"" + output_filename +
                              "\" \"" + config->hyper_file_name + "\""; }
                    else { cmd_str = std::string(FFMPEG_PATH) + " -i \"" + current_input_file + "\"" +
                              " -af \"" + std::string(filter_complex_a) + "\"" +
                              audio_opts_str + " -y \"" + output_filename + "\""; }
                }

                log_current_time(log_file);
                fprintf(log_file, "Run %d: Launching ffmpeg for input: '%s'\n", run, current_input_file.c_str());
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

    // Clean up temporary shuffled files
    for (const auto& temp_file : temp_files_to_delete) {
        remove(temp_file.c_str());
    }
    temp_files_to_delete.clear();
    
    if (config->create_hyper_file=='y') {
        std::string list_path = join_path(config->output_dir, "list.txt");
        FILE *list = fopen(list_path.c_str(), "w");
        if (list) {
            for (int i = 0; i < config->num_runs; i++) {
                int run = config->runNumber + i;
                for (size_t j = 0; j < config->input_files.size(); ++j) {
                    if (config->layer_files=='y') {
                        const std::string& output_ext = has_video_stream(config->input_files[0], config, log_file) ? config->video_output_extension : config->audio_output_extension;
                        std::string layered_filename = "layered_output_run" + std::to_string(run) + "." + output_ext;
                        fprintf(list, "file '%s'\n", join_path(config->output_dir, layered_filename).c_str());
                        break;
                    } else {
                        const std::string& output_ext = has_video_stream(config->input_files[j], config, log_file) ? config->video_output_extension : config->audio_output_extension;
                        std::string base_filename = get_basename(config->input_files[j]);
                        std::string per_file_filename = base_filename + "_run" + std::to_string(run) + "_file" + std::to_string(j) + "." + output_ext;
                        // Use %zu for size_t type 'j' to fix format mismatch warning/error.
                        fprintf(list, "file '%s'\n",
                                join_path(config->output_dir, per_file_filename).c_str());
                    }
                }
            }
            fclose(list);
        }
        
        std::string hyper_file_path = join_path(config->output_dir, config->hyper_file_name);
        std::string concat_cmd_str = std::string(FFMPEG_PATH) + " -f concat -safe 0 -i \"" + list_path + "\" -c copy \"" + hyper_file_path + "\"";

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
            remove(list_path.c_str()); // Clean up the list file
        }
    }
    fclose(log_file);
}

void print_help(const char* app_name) {
    std::cout << "Usage: " << app_name << " [options] [input_file1 input_file2 ...]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --layer-files               Layer all inputs into a single output file per run.\n";
    std::cout << "  --remix                     Enable remixing (sample rearrangement).\n";
    std::cout << "  --no-remix                  Disable remixing (sample rearrangement).\n";
    std::cout << "  --remix-intensity <f>       Set remix intensity (0.05-100). Lower is less shuffling.\n";
    std::cout << "  --remix-seed <int>          Set the seed for remixing (0 for random).\n";
    std::cout << "  --no-layer-files            Do not layer inputs into a single output file per run.\n";
    std::cout << "  -h, --help                  Show this help message.\n";
    std::cout << "  --create-hyper-file         Flag to create a concatenated hyper file.\n";
    std::cout << "  --no-hyper-file             Do not create a concatenated hyper file.\n";
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
    std::cout << "  --video-res <WxH>           Set the output video resolution (e.g., 1920x1080).\n";
    std::cout << "  --video-fps <rate>          Set the output video framerate (e.g., 30).\n";
    std::cout << "  --audio-ext <ext>           Set the output extension for audio files (default: mp3).\n";
    std::cout << "  -r, --runs <int>              Set the number of processing runs.\n";
    std::cout << "  -c, --channels <int>          Set the number of output audio channels.\n";
    std::cout << "  --starting-run, --starting-run-number <int>  Set the starting run number (default: 1).\n";
    std::cout << "  -s, --seed <int>              Set the random seed (0 for random).\n";
    std::cout << "  --sf2 <path>                Path to the SF2 SoundFont file for MIDI synthesis.\n";
    std::cout << "  --output-dir <path>         Set the directory for all output files.\n";
    std::cout << "  --midi <path>               Path to the MIDI file to synthesize and layer.\n";
    std::cout << "\nIf options are not provided, you will be prompted for them interactively.\n";
    std::cout << "Note: --sf2 and --midi require 'fluidsynth' to be installed and in your system's PATH.\n";
}


/**
 * @brief Expands a path pattern containing wildcards (*, ?) into a list of matching files.
 *
 * @param path_pattern The path pattern to expand (e.g., "data\*.mp4").
 * @return A vector of strings containing the matched file paths.
 */
std::vector<std::string> expand_wildcards(const std::string& path_pattern) {
    std::vector<std::string> files;

    // If the path doesn't contain a wildcard, just return it as is.
    if (path_pattern.find_first_of("*?") == std::string::npos) {
        files.push_back(path_pattern);
        return files;
    }

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(path_pattern.c_str(), &find_data);

    if (hFind != INVALID_HANDLE_VALUE) {
        // Extract the directory part from the pattern to prepend to filenames.
        size_t last_slash_pos = path_pattern.find_last_of("/\\");
        std::string dir_part = (last_slash_pos != std::string::npos) ? path_pattern.substr(0, last_slash_pos + 1) : "";

        do {
            // Ignore directories, only add files.
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(dir_part + find_data.cFileName);
            }
        } while (FindNextFileA(hFind, &find_data) != 0);
        FindClose(hFind);
    }
#else // POSIX implementation using glob
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    int return_value = glob(path_pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    if (return_value == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
            files.push_back(std::string(glob_result.gl_pathv[i]));
        }
    }
    globfree(&glob_result);
#endif

    return files;
}

#ifndef TESTING
int main(int argc, char *argv[]) {    YoloConfig config;
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
    // If no arguments are provided (other than the app name), go straight to interactive mode.
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "man") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--layer-files") {
            config.layer_files = 'y';
        } else if (arg == "--remix") {
            config.remix_enabled = 'y';
        } else if (arg == "--no-remix") {
            config.remix_enabled = 'n';
        } else if (arg == "--remix-intensity" && i + 1 < argc) {
            config.remix_intensity = std::stof(argv[++i]);
        } else if (arg == "--remix-seed" && i + 1 < argc) {
            config.remix_seed = std::stoi(argv[++i]); // This will now correctly handle 0
        } else if (arg == "--no-layer-files") {
            config.layer_files = 'n';
        } else if (arg == "--create-hyper-file") {
            config.create_hyper_file = 'y';
        } else if (arg == "--no-hyper-file") {
            config.create_hyper_file = 'n';
        } else if ((arg == "--hyper-filename" || arg == "--hyper") && i + 1 < argc) {
            config.hyper_file_name = argv[++i];
            config.create_hyper_file = 'y';
        } else if ((arg == "-r" || arg == "--runs")) {
            if (i + 1 < argc) {
                // Handle empty string argument for --runs
                if (strlen(argv[i+1]) == 0) {
                    std::cerr << "Error: Option '" << arg << "' requires a non-empty integer argument.\n";
                    return 1;
                }
                try {
                    config.num_runs = std::stoi(argv[++i]);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Error: Invalid argument for " << arg << ". Expected an integer, but got '" << argv[i] << "'.\n";
                    return 1; // Exit with an error code
                } catch (const std::out_of_range& e) {
                    std::cerr << "Error: Value for " << arg << " is out of range: '" << argv[i] << "'.\n";
                    return 1; // Exit with an error code
                }
            } else {
                std::cerr << "Error: Option '" << arg << "' requires an argument.\n";
                return 1; // Exit with an error code
            }
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
        } else if (arg == "--video-res" && i + 1 < argc) {
            config.video_res = argv[++i];
        } else if (arg == "--video-fps" && i + 1 < argc) {
            config.video_fps = argv[++i];
        } else if (arg == "--audio-ext" && i + 1 < argc) {
            config.audio_output_extension = argv[++i];
            config.audio_output_extension_is_default = false;
        } else if ((arg == "-c" || arg == "--channels") && i + 1 < argc) {
            config.num_audio_channels = std::stoi(argv[++i]);
        } else if ((arg == "--starting-run" || arg == "--starting-run-number") && i + 1 < argc) {
            config.runNumber = std::stoi(argv[++i]);
            config.runNumber_is_default = false;
        } else if ((arg == "-s" || arg == "--seed") && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        } else if (arg == "--output-dir" && i + 1 < argc) {
            config.output_dir = argv[++i];
        } else if (arg == "--sf2" && i + 1 < argc) {
            config.sf2_path = argv[++i];
        } else if (arg == "--midi" && i + 1 < argc) {
            config.midi_path = argv[++i];
        } else if (arg == "--no-midi") {
            config.midi_path = "disabled"; // Use a special string to indicate disabled
            config.sf2_path = "disabled";
        } else if (arg[0] == '-') {
            std::cerr << "Warning: Unknown option '" << arg << "' ignored." << std::endl;
        } else {
            // Treat as input file
            std::vector<std::string> expanded_files = expand_wildcards(argv[i]);
            for (const auto& file : expanded_files) {
                if (config.input_files.size() < MAX_FILES) {
                    config.input_files.push_back(file);
                } else {               
                    std::cerr << "Warning: Maximum number of input files (" << MAX_FILES << ") reached. Ignoring '" << file << "'." << std::endl;
                    break;
                }
            }
        }
        }
    }

    // --- Input File Handling ---
    if (config.input_files.empty()) {
        // No files provided via args, prompt the user
        std::cout << "No input files provided. Please enter one or more file paths." << std::endl;
        std::cout << "Use quotes for paths with spaces (e.g., \"C:\\My Videos\\file 1.mp4\")." << std::endl;
        std::cout << "Enter an empty line when finished, or 'exit' to quit." << std::endl;

        auto split_quoted = [](const std::string& line) -> std::vector<std::string> {
            std::vector<std::string> tokens;
            std::string cur;
            bool in_quotes = false;
            for (char ch : line) {
                if (ch == '"') {
                    in_quotes = !in_quotes;      // toggle quote state
                    cur += ch;                    // keep the quote for later stripping
                } else if (std::isspace(static_cast<unsigned char>(ch)) && !in_quotes) {
                    if (!cur.empty()) {
                        tokens.push_back(cur);
                        cur.clear();
                    }
                } else {
                    cur += ch;
                }
            }
            if (!cur.empty()) tokens.push_back(cur);
            return tokens;
        };

        std::string line;
        while (config.input_files.size() < MAX_FILES) {
            std::cout << "> ";
            std::getline(std::cin, line);
            if (line == "exit") {
                std::cout << "Exiting as requested." << std::endl;
                return 0;
            }
            if (line.empty()) {
                break;
            }

            for (const std::string& raw_token : split_quoted(line)) {
                std::string token = raw_token;
                trim_and_unquote(token);

                if (token == "exit") {
                    std::cout << "Exiting as requested." << std::endl;
                    return 0;
                }

                std::vector<std::string> expanded_files = expand_wildcards(token);
                if (expanded_files.empty() || (expanded_files.size() == 1 && expanded_files[0] == token)) {
                    bool already = std::any_of(config.input_files.begin(),
                                               config.input_files.end(),
                                               [&](const std::string& existing) {
                                                   return existing == token;
                                               });
                    if (already) {
                        std::cout << "Argument '" << token
                                  << "' provided as input filename, but it was already specified.\n";
                        continue;
                    }
                    if (config.input_files.size() < MAX_FILES) {
                        config.input_files.push_back(token);
                    }
                } else {
                    for (const auto& file : expanded_files) {
                        bool already = std::any_of(config.input_files.begin(),
                                                   config.input_files.end(),
                                                   [&](const std::string& existing) {
                                                       return existing == file;
                                                   });
                        if (already) {
                            std::cout << "Argument '" << file
                                      << "' provided as input filename, but it was already specified.\n";
                            continue;
                        }
                        if (config.input_files.size() < MAX_FILES) {
                            config.input_files.push_back(file);
                        } else {
                            break;
                        }
                    }
                }
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

    try {
        get_other_config(&config, &seed);
        run_yolo_process(&config, seed);
    } catch (const UserExitException&) {
        std::cout << "Exiting as requested." << std::endl;
        return 0;
    }
    return 0;
}
#endif

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
