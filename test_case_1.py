import subprocess
import os
import sys

YOLO_EXE = "./yolo.exe" if os.name == 'nt' else "./yolo"


def run_yolo_command(args, test_name, stdin_input=None):
    """Helper to run a yolo command and check the result."""
    cmd = [YOLO_EXE] + args
    print(f"  Running command: {' '.join(cmd)}")
    # Use Popen to stream output in real-time, preventing the appearance of a stall.
    # We set stdout and stderr to PIPE and then read from them.
    # However, a simpler approach for immediate feedback is to let the subprocess
    # inherit the parent's stdout/stderr, so its output appears directly.
    # We will check the return code to determine success or failure.
    try:
        process = subprocess.run(cmd, check=True, text=True, capture_output=False, input=stdin_input)
        print(f"  YOLO command completed successfully.")
        return True
    except subprocess.CalledProcessError as e:
        # The output from the failed command will have already been printed to the console.
        print(f"  [FAIL] {test_name}: YOLO command failed with exit code {e.returncode}")
        return False
    except FileNotFoundError:
        print(f"  [FAIL] {test_name}: Could not find the executable '{YOLO_EXE}'. Please ensure it is built and in the correct path.")
        return False

def check_output_file(filename, should_exist=True):
    """Helper to check for an output file's existence."""
    if should_exist and not os.path.exists(filename):
        print(f"  [FAIL] Output file '{filename}' was not created.")
        return False
    if not should_exist and os.path.exists(filename):
        print(f"  [FAIL] Output file '{filename}' was created but should not have been.")
        return False
    return True

def test_single_audio_file():
    """Scenario 2: Single File Processing (Audio)"""
    test_name = "test_single_audio_file"
    output_file = "test_audio.wav_run1_file0.mp3"
    args = [
        "test_media/test_audio.wav", "--runs", "1", "--seed", "1", "--starting-run", "1", "--output-dir", "test_output",
        "--no-layer-files", "--no-hyper-file", "--no-remix", "--no-midi", "--channels", "2", "--audio-ext", "mp3",
        "-q", "5", "-bb", "0", "-tg", "0", "-l", "-23", "-t", "1.0"
    ]
    if not run_yolo_command(args, test_name): return False
    if not check_output_file(os.path.join("test_output", output_file)): return False
    return True

def test_single_video_file():
    """Scenario 2: Single File Processing (Video)"""
    test_name = "test_single_video_file"
    output_file = "test_video.mp4_run1_file0.mp4"
    args = [
        "test_media/test_video.mp4", "--runs", "1", "--seed", "1", "--starting-run", "1", "--output-dir", "test_output",
        "--no-layer-files", "--no-hyper-file", "--no-remix", "--no-midi", "--channels", "2", "--audio-ext", "mp3", "--video-ext", "mp4",
        "-q", "20", "-bb", "0", "-tg", "0", "-l", "-23", "-t", "1.0",
        "-bt", "0", "-ct", "1", "-st", "1", "--video-res", "320x240", "--video-fps", "30"
    ]
    if not run_yolo_command(args, test_name): return False
    if not check_output_file(os.path.join("test_output", output_file)): return False
    return True

def test_layering():
    """Scenario 4: Layering Mode"""
    test_name = "test_layering"
    output_file = "layered_output_run1.mp4"
    args = [
        "test_media/test_audio.wav", "test_media/test_video.mp4", "--runs", "1", "--seed", "1", "--starting-run", "1",
        "--output-dir", "test_output", "--layer-files", "--no-hyper-file", "--no-remix", "--no-midi",
        "--channels", "2", "--audio-ext", "mp3", "--video-ext", "mp4", "-q", "20", "-bb", "0", "-tg", "0", "-l", "-23", "-t", "1.0",
        "-bt", "0", "-ct", "1", "-st", "1", "--video-res", "320x240", "--video-fps", "30"
    ]
    if not run_yolo_command(args, test_name): return False
    if not check_output_file(os.path.join("test_output", output_file)): return False
    return True

def test_remixing():
    """Scenario 5: Remixing Mode"""
    test_name = "test_remixing"
    output_file = "test_audio.wav_run1_file0.mp3"
    args = [
        "test_media/test_audio.wav", "--runs", "1", "--seed", "42", "--starting-run", "1", "--output-dir", "test_output",
        "--no-layer-files", "--no-hyper-file", "--remix", "--no-midi", "--remix-intensity", "50", "--remix-seed", "42",
        "--channels", "2", "--audio-ext", "mp3", "-q", "5", "-bb", "0", "-tg", "0", "-l", "-23", "-t", "1.0"
    ]
    if not run_yolo_command(args, test_name): return False
    if not check_output_file(os.path.join("test_output", output_file)): return False
    return True

def test_hyper_file():
    """Scenario 6: Hyper-File Creation"""
    test_name = "test_hyper_file"
    hyper_file = "master_mix.mp3"
    args = [
        "test_media/test_audio.wav", "--runs", "2", "--seed", "1", "--starting-run", "1", "--output-dir", ".",
        "--no-layer-files", "--hyper-filename", hyper_file, "--no-remix", "--no-midi",
        "--channels", "2", "--audio-ext", "mp3", "-q", "5", "-bb", "0", "-tg", "0", "-l", "-23", "-t", "1.0"
    ]
    if not run_yolo_command(args, test_name, stdin_input="master_mix.mp3\n"): return False
    if not check_output_file(hyper_file): return False
    return True

def test_help_flag():
    """Scenario 9: Help and Invalid Arguments (--help)"""
    test_name = "test_help_flag"
    cmd = [YOLO_EXE, "--help"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  [FAIL] {test_name}: --help flag failed with exit code {result.returncode}")
        return False
    if "Usage:" not in result.stdout:
        print(f"  [FAIL] {test_name}: --help output did not contain 'Usage:'")
        return False
    return True

def test_invalid_arg_value():
    """Scenario 9: Help and Invalid Arguments (invalid value)"""
    test_name = "test_invalid_arg_value"
    cmd = [YOLO_EXE, "--runs", "not_a_number"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode == 0:
        print(f"  [FAIL] {test_name}: Command succeeded but should have failed.")
        return False
    if "Error: Invalid argument for --runs" not in result.stderr:
        print(f"  [FAIL] {test_name}: Stderr did not contain expected error message.")
        print(f"  Stderr: {result.stderr}")
        return False
    return True

def test_missing_arg_value():
    """Scenario 9: Help and Invalid Arguments (missing value)"""
    test_name = "test_missing_arg_value"
    cmd = [YOLO_EXE, "--runs"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode == 0:
        print(f"  [FAIL] {test_name}: Command succeeded but should have failed.")
        return False
    if "Error: Option '--runs' requires an argument" not in result.stderr:
        print(f"  [FAIL] {test_name}: Stderr did not contain expected error message.")
        print(f"  Stderr: {result.stderr}")
        return False
    return True


def test_fully_interactive_mode_no_args():
    """Scenario 1: Fully Interactive Mode (No CLI args)"""
    test_name = "test_fully_interactive_mode_no_args"
    output_file = "test_audio.wav_run1_file0.mp3"

    # This string simulates a user typing answers to every prompt.
    # Each answer is followed by a newline character.
    interactive_input = (
        "test_media/test_audio.wav\n"  # Input file prompt
        "\n"                           # Empty line to finish file input
        "n\n"                          # Generate layered files?
        "n\n"                          # Create a hyper file?
        "n\n"                          # Enable remixing?
        "0\n"                          # Bass boost
        "0\n"                          # Treble gain
        "-23\n"                        # Volume LUFS
        "1.0\n"                        # Tempo modifier
        "5\n"                          # Quality
        "2\n"                          # Output audio channels
        "mp3\n"                        # Audio output extension
        "\n"                           # Path to MIDI file (leave blank to skip)
        "test_output\n"                # Output directory
        "1\n"                          # Number of runs
        "1\n"                          # Random seed
    )

    # Run the command with no arguments, feeding it the scripted input.
    if not run_yolo_command([], test_name, stdin_input=interactive_input):
        return False
    if not check_output_file(os.path.join("test_output", output_file)):
        return False
    return True


ALL_TESTS = [
    test_single_audio_file,
    test_single_video_file,
    test_layering,
    test_remixing,
    test_hyper_file,
    test_help_flag,
    test_invalid_arg_value,
    test_missing_arg_value,
    test_fully_interactive_mode_no_args,
]
