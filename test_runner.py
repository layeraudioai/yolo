import subprocess
import os
import sys
import shutil
import test_case_1

def setup_test_environment():
    """Create dummy media files and output directories for testing."""
    print("--- Setting up test environment ---")
    media_dir = "test_media"
    output_dir = "test_output"
    
    # Clean up previous runs
    if os.path.exists(media_dir):
        shutil.rmtree(media_dir)
    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)
        
    os.makedirs(media_dir, exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)
    
    try:
        # Create a dummy stereo audio file
        subprocess.run(["ffmpeg", "-f", "lavfi", "-i", "sine=d=1:f=440", "-y", os.path.join(media_dir, "test_audio.wav")], check=True, capture_output=True)
        # Create a dummy video file with audio
        subprocess.run(["ffmpeg", "-f", "lavfi", "-i", "testsrc=d=1:s=320x240", "-f", "lavfi", "-i", "sine=d=1:f=660", "-y", os.path.join(media_dir, "test_video.mp4")], check=True, capture_output=True)
        
        # Create a dummy MIDI file using the python-midi library
        try:
            import midi
            pattern = midi.Pattern()
            track = midi.Track()
            pattern.append(track)
            track.append(midi.NoteOnEvent(tick=0, velocity=100, pitch=midi.C_5))
            track.append(midi.NoteOffEvent(tick=200, pitch=midi.C_5))
            track.append(midi.EndOfTrackEvent(tick=1))
            midi.write_midifile(os.path.join(media_dir, "test.mid"), pattern)
        except ImportError:
            print("Warning: 'python-midi' not found. Run 'pip install python-midi' to enable MIDI tests.", file=sys.stderr)
            # Fallback to writing a minimal raw MIDI file if the library is not installed.
            with open(os.path.join(media_dir, "test.mid"), "wb") as f:
                f.write(b'MThd\x00\x00\x00\x06\x00\x01\x00\x01\x01\xe0MTrk\x00\x00\x00\x0c\x00\x90<\x7f\x81\x00\x80<\x7f\x00\xff/\x00')

        # Create a dummy SF2 file (just a header, not a valid soundfont)
        with open(os.path.join(media_dir, "test.sf2"), "wb") as f:
            f.write(b'RIFF\x00\x00\x00\x00sfbkLIST\x00\x00\x00\x00pdta')
        print("Dummy media files created successfully.")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print("Error: Could not create dummy files with ffmpeg. Ensure ffmpeg is in your PATH.", file=sys.stderr)
        print(e, file=sys.stderr)
        return False

def cleanup_test_environment():
    """Remove dummy files and directories."""
    print("\n--- Cleaning up test environment ---")
    if os.path.exists("test_media"):
        shutil.rmtree("test_media")
    if os.path.exists("test_output"):
        shutil.rmtree("test_output")
    # if os.path.exists("yolo.log"):
    #     os.remove("yolo.log")
    print("Cleanup complete.")

def run_all_tests():
    yolo_exe = "yolo.exe" if os.name == 'nt' else "yolo"
    if not os.path.exists(yolo_exe):
        print(f"Error: {yolo_exe} not found. Make sure it is built and in the current directory.", file=sys.stderr)
        return 1

    if not setup_test_environment():
        return 1

    failures = 0
    for test_func in test_case_1.ALL_TESTS:
        print(f"\n--- Running test: {test_func.__name__} ---")
        # Reset output directory for each test
        if os.path.exists("test_output"):
            shutil.rmtree("test_output")
        os.makedirs("test_output")

        if not test_func():
            failures += 1
            print(f"--- [FAIL] {test_func.__name__} ---")
        else:
            print(f"--- [PASS] {test_func.__name__} ---")

    cleanup_test_environment()

    if failures > 0:
        print(f"\n[SUMMARY] {failures} test(s) failed.")
        return 1
    else:
        print("\n[SUMMARY] All tests passed.")
        return 0

if __name__ == "__main__":
    sys.exit(run_all_tests())
