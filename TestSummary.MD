Unit (function) Tests:
----------------------
void get_other_config(YoloConfig *config, int *seed);
examples: (Requires mocking stdin/stdout, `has_video_stream`, and filesystem)
- All config values are default: User is prompted for every value and provides valid input. Verify all `config` fields are updated correctly.
- Some config values are pre-set via CLI args: User is only prompted for the remaining default values.
- Invalid input followed by valid input: For numeric prompts, enter a string first, then a valid number. Verify the function handles it gracefully and re-prompts.
- Invalid path input: User provides a path with invalid characters for `output_dir`. Verify behavior (e.g., directory creation fails later).
- EOF (Ctrl+D/Ctrl+Z) during prompt: Verify the application handles this without entering an infinite loop and exits gracefully.
- Boundary value input: For `quality`, enter 0, 31, -1, and 32. Verify it accepts the valid range.
- Conditional prompts:
  - `create_hyper_file` is 'y': Verify `hyper_file_name` is prompted.
  - `create_hyper_file` is 'n': Verify `hyper_file_name` is NOT prompted.
  - `remix_enabled` is 'y': Verify `remix_seed` and `remix_intensity` are prompted.
  - `remix_enabled` is 'n': Verify remix options are NOT prompted.
  - `input_files` contains a video file: Verify video-specific options (brightness, contrast, etc.) are prompted.
  - `input_files` contains only audio files: Verify video-specific options are NOT prompted.
- Empty input for paths: User provides an empty string for `output_dir`. Verify it defaults to ".".
- Path input: User provides a path that does not exist for `output_dir`. Verify the directory is created later by `run_yolo_process`.
- `remix_seed` logic:
  - `remix_seed` is 0 (default): Verify user is prompted for a seed.

void run_yolo_process(YoloConfig *config, int seed);
examples: (Integration-style tests, requires real/mocked ffmpeg, ffprobe, fluidsynth)
- Single audio file, no layering, no remix.
- Single video file, no layering, no remix.
- Multiple audio files, layered into one output.
- Multiple video files, layered into one output.
- Multiple mixed audio/video files, layered into one output.
- Single audio file with remix enabled.
- Single video file with remix enabled.
- Multiple files with remix and layering enabled.
- MIDI and SF2 files provided: Verify fluidsynth is called and its output is included in layering.
- `create_hyper_file` is 'y': Verify a `list.txt` is created and the final concatenation command is run.
- `num_runs` > 1: Verify the process loops the correct number of times with different seeds.
- Zero input files: The function should handle this gracefully (likely doing nothing).
- Invalid file paths in input: Verify ffmpeg errors are logged but the overall process doesn't crash.
- File with unsupported codec: Verify ffmpeg logs an error and the process continues with other files.
- Layering with only one input file: Verify it processes the single file correctly without amerge/blend.
- Remixing with very low intensity (e.g., 0.05): Verify only a small number of chunks are shuffled.
- Remixing with very high intensity (e.g., 100): Verify most/all chunks are shuffled.
- Remixing a file with no detectable onsets: Verify it falls back to fixed-duration chunking.
- Remixing with transformations: Verify temporary transformed chunk files are created and then deleted.
- Layering with inputs of different channel counts (e.g., mono + 5.1). Verify `get_pan_filter_string` is called with the correct total channel count.
- Layering with inputs of different resolutions. Verify the output resolution matches the first input or a specified target.
- Layering with audio-only and video-only files. Verify audio and video are correctly mapped and merged.
- Layering with no audio inputs: Verify the final command includes `-an` (no audio).
- Layering with no video inputs: Verify the final command does not include `-map "[v_out]"`.
- Per-file processing with specific audio codecs for video output (`-c 8` -> flac, `-c 16` -> pcm_s24le).
- Hyper-file creation with layering: Verify `list.txt` contains one entry per run for the layered output file.
- Per-file processing with specific output formats (`--audio-ext opus`, `--audio-ext wav`). Verify the correct ffmpeg command-line options are used.
- `MAX_PROCESSES` limit reached: Launch more processes than the limit. Verify the application waits for existing processes to finish before launching new ones, rather than crashing.
- Failure during temporary file creation (e.g., due to permissions). Verify the error is logged and the process for that file is skipped.
- Audio-only output format constraints: Process a video file but output to `.mp3`. Verify the final command includes `-ac 2` and no video stream (`-vn`).
- `output_dir` specified: Verify all output files and logs are created in the correct directory.

std::string get_pan_filter_string(int num_input_channels, int num_output_channels);
examples:
- `get_pan_filter_string(2, 1)` -> `pan=mono|c0=c0`
- `get_pan_filter_string(2, 2)` -> Starts with `pan=stereo`, contains `|c0=` and `|c1=`.
- `get_pan_filter_string(2, 6)` -> Starts with `pan=5.1`, contains `|c0=` through `|c5=`.
- `get_pan_filter_string(2, 8)` -> Starts with `pan=7.1`.
- `get_pan_filter_string(2, 16)` -> Starts with `pan=hexadecagonal`.
- `get_pan_filter_string(4, 3)` -> Starts with `pan=3`.
- `get_pan_filter_string(0, 2)` -> `""`
- `get_pan_filter_string(2, 0)` -> `""`
- `get_pan_filter_string(-1, 2)` -> `""`
- `get_pan_filter_string(2, -1)` -> `""`
- `get_pan_filter_string(1, 8)` -> Starts with `pan=7.1`, verify fallback logic for channel mixing.
- `get_pan_filter_string(6, 2)` -> Starts with `pan=stereo`, verify downmixing logic.
- `get_pan_filter_string` with mocked `rand()`: Force a scenario where no input channels are randomly selected for an output channel, and verify the fallback logic (`c0=c0`, `c1=c1`, etc.) is correctly applied.
- Verify random output contains valid syntax: The generated string for multi-channel output should not end with `+` or `-`.

std::vector<char*> get_argv(const std::string& cmd, std::vector<std::string>& storage);
examples: (Note: This function is POSIX-only and currently unused)
- `get_argv("ffmpeg -i file.mp4 out.mkv", storage)` -> `storage` contains {"ffmpeg", "-i", "file.mp4", "out.mkv"}, returned vector points to these strings.
- `get_argv("", storage)` -> `storage` is empty, returned vector contains only `nullptr`.
- `get_argv("cmd with  extra   spaces", storage)` -> `storage` contains {"cmd", "with", "extra", "spaces"}.

void log_current_time(FILE *f) ;
examples: (Requires file I/O)
- Call `log_current_time` with a `nullptr`. Verify it does not crash. (Note: Current implementation would crash, suggesting a null check is needed).
- Call `log_current_time` on an open file pointer. Read the file and verify the output format matches `[YYYY-MM-DD HH:MM:SS] `.

bool has_video_stream(const std::string& filename, YoloConfig* config, FILE* log_file);
examples: (Requires mocked or real ffprobe)
- Input is a video file (e.g., .mp4, .mkv). Should return `true`.
- Input is an audio file with embedded cover art (e.g., .mp3, .flac). Should return `true`.
- Input is a pure audio file with no video stream. Should return `false`.
- Input is a non-existent file. Should return `false` and log an error.
- Call twice with the same filename. Verify the second call is served from cache (i.e., ffprobe is not called again).
- Cache invalidation (conceptual): If a file's modification time changes, the cache should ideally be invalidated. (This is a feature suggestion based on testing).
- Security: Input filename contains shell metacharacters (e.g., `my_file;reboot;.mp4`). Verify the command quoting prevents command injection.
- Input is a file path with spaces or special characters. Verify the command is quoted correctly and ffprobe succeeds.

int get_audio_channel_count(const std::string& filename, FILE* log_file);
examples: (Requires mocked or real ffprobe)
- Input is a stereo file. Should return `2`.
- Input is a 5.1 surround file. Should return `6`.
- Input is a mono file. Should return `1`.
- Input is a video file with no audio. Should return `2` (due to fallback logic). This is a known behavior to test.
- Input is a file with multiple audio streams. Verify it returns the channel count of the first stream (`a:0`).
- Input is a non-existent file. Should return the fallback value `2`.
- `get_audio_channel_count` on Windows with a very long file path (>260 chars). Verify `CreateProcessA` handles it or fails gracefully.
- Input is a corrupted file. Verify it fails gracefully and returns the fallback value `2`.

std::string get_basename(const std::string& path);
examples:
- `get_basename("path/to/file.txt")` -> `"file.txt"`
- `get_basename("C:\\Users\\Test\\video.mp4")` -> `"video.mp4"`
- `get_basename("file.txt")` -> `"file.txt"`
- `get_basename("")` -> `""`
- `get_basename("path/to/")` -> `""`
- `get_basename("path/to/.")` -> `"."`
- `get_basename("path/to/..")` -> `".."`

void print_help(const char* app_name);
examples: (Requires mocking stdout)
- Call `print_help("my_app")`. Capture output and verify it contains the usage string "Usage: my_app [...]" and lists several options.

template<typename T> void prompt_for_value (const std::string& prompt, T& value);
examples: (Requires mocking stdin/stdout)
- `prompt_for_value<int>`: Provide valid integer input. Verify the value is read correctly.
- `prompt_for_value<float>`: Provide valid float input.
- `prompt_for_value<std::string>`: Provide a string with spaces. Verify `getline` is used and the whole line is read.
- `prompt_for_value<int>`: Provide invalid text input, then a valid integer. Verify it re-prompts and eventually succeeds.

std::vector<std::string> expand_wildcards(const std::string& path_pattern);
examples: (Requires filesystem interaction)
- Create files `test1.txt`, `test2.txt`, `another.dat`.
- `expand_wildcards("*.txt")` -> Returns a vector containing "test1.txt" and "test2.txt".
- `expand_wildcards("test?.txt")` -> Returns a vector containing "test1.txt" and "test2.txt".
- `expand_wildcards("*.log")` -> Returns an empty vector.
- `expand_wildcards("non_existent_dir/*.txt")` -> Returns an empty vector.
- `expand_wildcards("path/with spaces/*.txt")` -> Verify it handles paths with spaces correctly.
- `expand_wildcards("another.dat")` -> Returns a vector with one element: "another.dat" (no wildcard).

ShuffledMedia shuffle_media_file(...);
examples: (Integration-style test requiring real ffmpeg/ffprobe)
- Shuffle an audio-only file. Verify `result.audio_file` is populated, `result.video_file` is empty, and `success` is true.
- Shuffle a video file (with audio). Verify both `audio_file` and `video_file` are populated.
- Shuffle a silent video file. Verify `pcm_data` is empty and the function returns a failure state.
- Shuffle a file with high remix intensity. Verify transformation filters (like `areverse`, `atempo`) are applied to chunks.
- Shuffle a file with a non-existent path. Verify it fails gracefully and returns `success = false`.
- Shuffle a file that ffmpeg cannot decode. Verify it fails gracefully.
- Shuffle a file where video and audio streams have different durations. Verify chunking logic handles this without crashing.
- Verify all temporary chunk files and the concat list file are deleted after the process.
------------------------------------------------------------------------------------------------
Runtime Tests (UI):
-------------------
### Scenario 1: Fully Interactive Mode
- **Test Case:** Run the executable with no command-line arguments.
- **Action:** The application should prompt for input files. Enter one audio and one video file. Then, answer all subsequent prompts for configuration with valid values.
- **Expected Result:** The application runs successfully, creating one output file for each input file per run, and logs its actions to `yolo.log`.

### Scenario 2: Single File Processing
- **Test Case:** Run with a single input file path as an argument.
- **Sub-cases:**
  - `yolo.exe "audio.mp3"` (pure audio)
  - `yolo.exe "song_with_art.mp3"` (audio with embedded image)
  - `yolo.exe "video.mp4"` (video with audio)
  - `yolo.exe "silent_video.mkv"` (video without audio)
- **Action:** Answer interactive prompts for the remaining configuration.
- **Expected Result:** A single output file is generated per run, with the correct extension (`.mp3` for pure audio, `.mkv` for others by default). The process should correctly identify which files have video streams.

### Scenario 3: Multi-File Processing with Wildcards
- **Test Case:** Run with a wildcard path.
- **Action:** `yolo.exe "media/*.mp4"`. Answer interactive prompts.
- **Expected Result:** All `.mp4` files in the `media` directory are processed individually, creating multiple output files.

### Scenario 4: Layering Mode
- **Test Case:** Run with multiple files and the `--layer-files` flag.
- **Sub-cases:**
  - `yolo.exe --layer-files "audio1.mp3" "audio2.ogg"`
  - `yolo.exe --layer-files "video1.mp4" "video2.webm"`
  - `yolo.exe --layer-files "video.mp4" "audio.mp3"`
- **Action:** Provide remaining config interactively.
- **Expected Result:** A single output file (`layered_output_runN.ext`) is created per run. The extension should be for video if any input has a video stream. The audio from all inputs should be mixed. The video from all inputs should be blended.

### Scenario 5: Remixing Mode
- **Test Case:** Run with `--remix` and related flags.
- **Sub-cases:**
  - `yolo.exe --remix --remix-intensity 50 "audio.mp3"`
  - `yolo.exe --remix --remix-seed 12345 "video.mp4"`
  - `yolo.exe --layer-files --remix --remix-intensity 10 "audio.mp3" "video.mp4"`
- **Action:** Provide remaining config interactively.
- **Expected Result:** The output files should have their audio (and video, if present) segments shuffled. Using the same seed should produce identical shuffled output. Higher intensity should result in more noticeable shuffling. Temporary shuffled files should be created and then deleted.

### Scenario 6: Hyper-File Creation
- **Test Case:** Run with `--create-hyper-file` or `--hyper-filename`.
- **Action:** `yolo.exe --runs 3 --hyper-filename "master_mix.mkv" "media/*.*"`
- **Expected Result:** Individual output files are created for each run/file combination. Afterwards, a `list.txt` file is generated, and `ffmpeg` is called to concatenate all outputs into `master_mix.mkv`. The `list.txt` file is then deleted.

### Scenario 7: MIDI Synthesis
- **Test Case:** Run with `--midi` and `--sf2` flags.
- **Action:** `yolo.exe --layer-files --midi "song.mid" --sf2 "soundfont.sf2" "video.mp4"`
- **Expected Result:** `fluidsynth` is called to generate a temporary `.wav` file from the MIDI. This `.wav` file is then included as an input in the layering process. The temporary file is deleted upon completion.

### Scenario 8: Full Command-Line Automation
- **Test Case:** Provide all necessary arguments on the command line to avoid all interactive prompts.
- **Action:** `yolo.exe "audio.mp3" --layer-files n --remix n --create-hyper-file n -bb 5 -tg 3 -l -14 -t 1.1 -q 2 -c 6 --runs 2 --seed 42 --output-dir "results"`
- **Expected Result:** The application runs without any user interaction, processes the file, and places all outputs in the `results` directory.

### Scenario 9: Help and Invalid Arguments
- **Test Case:** Test help flags and invalid argument handling.
- **Sub-cases:**
  - `yolo.exe -h`
  - `yolo.exe --help`
  - `yolo.exe --unknown-flag`
  - `yolo.exe --runs` (missing value)
  - `yolo.exe --runs not_a_number`
- **Expected Result:** Help flags should print the usage message and exit. Unknown flags should show a warning but continue. Missing or invalid arguments for flags should print an error and exit with a non-zero status code.

### Scenario 10: Fully Automated CLI-Only Scenarios
- **Test Case:** Test individual CLI flags in a non-interactive run. Each command should include `--runs 1 --seed 1` and an input file to prevent prompts.
- **Sub-cases (each is a separate run):**
  - Video Effects: `yolo.exe "video.mp4" --runs 1 --seed 1 -bt 1.2 -ct 1.5 -st 0.8` -> Verify `eq` filter is in the log.
  - Audio Effects: `yolo.exe "audio.mp3" --runs 1 --seed 1 -bb 5 -tg -3 -l -16 -t 1.1` -> Verify `bass`, `treble`, `volume`, `atempo` filters are in the log.
  - Video Output: `yolo.exe "video.mp4" --runs 1 --seed 1 --video-ext "webm" --video-res "640x480" --video-fps 24` -> Verify output is `video.mp4_run1_file0.webm` with correct properties.
  - Audio Output: `yolo.exe "audio.mp3" --runs 1 --seed 1 --audio-ext "opus" -c 1` -> Verify output is `.opus` and command uses opus-specific options.
  - Run Control: `yolo.exe "audio.mp3" --runs 3 --seed 123 --starting-run 10` (Also supports `--starting-run-number 10`) -> Verify outputs are `..._run10_...`, `..._run11_...`, `..._run12_...`.
  - Hyper File Alias: `yolo.exe "audio.mp3" --runs 1 --seed 1 --hyper "master.mkv"` -> Verify `create_hyper_file` is set to 'y' and the output is `master.mkv`.
  - Boolean Flags: `yolo.exe "audio.mp3" --runs 1 --seed 1 --no-remix --no-layer-files --no-hyper-file` -> Verify these modes are disabled in the log.

### Scenario 10: Layering with Mixed Media Types
- **Test Case:** Layering files with different properties.
- **Sub-cases:**
  - `yolo.exe --layer-files "mono_audio.wav" "surround_5.1.flac"`
  - `yolo.exe --layer-files "video_4k.mp4" "video_480p.webm"`
  - `yolo.exe --layer-files "video_with_audio.mp4" "silent_video.mkv" "audio_only.mp3"`
- **Action:** Set output channels to 6. Set output resolution to 1920x1080.
- **Expected Result:** A single layered file is created. The audio from all sources is correctly mixed into a 6-channel output. The video is blended and scaled to the target 1920x1080 resolution.

### Scenario 11: Filesystem and Path Edge Cases
- **Test Case:** Use file and directory paths with spaces and special characters.
- **Action:** `yolo.exe --output-dir "my results/" "C:\Users\Test User\My Media\funky&video.mp4"`
- **Expected Result:** The application correctly handles the quoted paths, finds the input file, and creates the output directory and files within it without error.

### Scenario 12: Resource Constraints (Manual Test)
- **Test Case:** Process a very large number of files or very large individual files.
- **Action:** `yolo.exe --runs 10 "large_media_collection/*.*"`
- **Expected Result:** The application should process all files without crashing due to memory exhaustion or hitting process limits. `MAX_FILES` and `MAX_PROCESSES` constants should prevent crashes, but may log warnings if limits are exceeded. Performance should be monitored.

### Scenario 12: Interactive Input Edge Cases
- **Test Case:** Provide tricky input during interactive prompts.
- **Action:** When prompted for input files, provide a path with wildcards like `"media/*.mp3"`. When prompted for a numeric value like `--runs`, enter text. When prompted for a path, press Enter to leave it blank.
- **Expected Result:** The application should correctly expand the wildcard path. It should re-prompt for the invalid numeric input. It should use the correct default for the blank path input.

### Scenario 14: File Permissions
- **Test Case:** Test behavior with incorrect file permissions.
- **Action:** Create a read-only directory and set it as `--output-dir`. Try to process an input file that the user does not have read permissions for.
- **Expected Result:** For the read-only output directory, the application should fail to open the log file and exit gracefully with an error message to stderr. For the unreadable input file, ffprobe/ffmpeg should fail, the error should be logged, and the application should skip that file and continue with others if possible.