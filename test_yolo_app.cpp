#include "gtest/gtest.h"
#include <fstream>
#include <filesystem>

#include "yolo_app.cpp"

// Test fixture for tests that require filesystem interaction
class FilesystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a simple path in the current directory, converted to absolute
       test_dir = (std::filesystem::current_path() / "test_dir").string();
        
        // Remove old dir if it exists to ensure a clean state
        std::filesystem::remove_all(test_dir);
        
        // Create it
        std::filesystem::create_directory(test_dir);
    }

    void TearDown() override {
        // Clean up: remove files and directory
        std::filesystem::remove_all(test_dir);
    }
    std::string test_dir;
};


TEST(YoloAppUnitTests, GetPanFilterString) {
    // Test case 1: Mono output
    EXPECT_EQ(get_pan_filter_string(2, 1), "pan=mono|c0=c0");

    // Test case 2: Stereo output from 2 channels (simple case)
    // The output is random, so we check for the static parts.
    std::string stereo_pan = get_pan_filter_string(2, 2);
    EXPECT_TRUE(stereo_pan.rfind("pan=stereo", 0) == 0); // Check if it starts with "pan=stereo"
    EXPECT_NE(stereo_pan.find("|c0="), std::string::npos);
    EXPECT_NE(stereo_pan.find("|c1="), std::string::npos);

    // Test case 3: 5.1 output from 2 channels
    std::string surround_pan = get_pan_filter_string(2, 6);
    EXPECT_TRUE(surround_pan.rfind("pan=5.1", 0) == 0);
    EXPECT_NE(surround_pan.find("|c0="), std::string::npos);
    EXPECT_NE(surround_pan.find("|c1="), std::string::npos);
    EXPECT_NE(surround_pan.find("|c2="), std::string::npos);
    EXPECT_NE(surround_pan.find("|c3="), std::string::npos);
    EXPECT_NE(surround_pan.find("|c5="), std::string::npos); // Check for the last channel

    // Test case 4: Hexadecagonal (16 channel) output
    std::string hexa_pan = get_pan_filter_string(2, 16);
    EXPECT_TRUE(hexa_pan.rfind("pan=hexadecagonal", 0) == 0);

    // Test case 5: Custom channel layout
    std::string custom_pan = get_pan_filter_string(4, 3);
    EXPECT_TRUE(custom_pan.rfind("pan=3", 0) == 0);
    EXPECT_NE(custom_pan.find("|c0="), std::string::npos);
    EXPECT_NE(custom_pan.find("|c1="), std::string::npos);
    EXPECT_NE(custom_pan.find("|c2="), std::string::npos);

    // Test case 6: Invalid inputs
    EXPECT_EQ(get_pan_filter_string(0, 2), "");
    EXPECT_EQ(get_pan_filter_string(2, 0), "");
    EXPECT_EQ(get_pan_filter_string(-1, 5), "");
    EXPECT_EQ(get_pan_filter_string(2, -1), "");

    // Test case 7: Downmixing
    std::string downmix_pan = get_pan_filter_string(6, 2);
    EXPECT_TRUE(downmix_pan.rfind("pan=stereo", 0) == 0);
    EXPECT_NE(downmix_pan.find("|c0="), std::string::npos);
    EXPECT_NE(downmix_pan.find("|c1="), std::string::npos);

    // Test case 8: Fallback logic for empty channel mix
    // Seed rand() to a known value to make the test deterministic.
    // This doesn't guarantee the fallback is hit, but it's a good attempt.
    srand(1);
    std::string fallback_pan = get_pan_filter_string(1, 8); // 1 input, 8 outputs
    EXPECT_TRUE(fallback_pan.rfind("pan=7.1", 0) == 0);
    EXPECT_NE(fallback_pan.find("|c7=c0"), std::string::npos); // Check if fallback c7=c(7%1) is present
}

TEST(YoloAppUnitTests, TrimAndUnquote) {
    std::string s;

    // Test leading/trailing whitespace
    s = "  hello world  ";
    trim_and_unquote(s);
    EXPECT_EQ(s, "hello world");

    // Test quotes
    s = "\"quoted string\"";
    trim_and_unquote(s);
    EXPECT_EQ(s, "quoted string");

    // Test quotes with whitespace
    s = "  \"  spaced quote  \"  ";
    trim_and_unquote(s);
    EXPECT_EQ(s, "  spaced quote  "); // Inner spaces are preserved

    // Test no change
    s = "clean";
    trim_and_unquote(s);
    EXPECT_EQ(s, "clean");

    // Test only whitespace
    s = "   ";
    trim_and_unquote(s);
    EXPECT_EQ(s, "");

    // Test only quotes
    s = "\"\"";
    trim_and_unquote(s);
    EXPECT_EQ(s, "");

    // Test empty string
    s = "";
    trim_and_unquote(s);
    EXPECT_EQ(s, "");
}

#include <algorithm>

std::string normalize_path(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

TEST(YoloAppUnitTests, JoinPath) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    std::string expected_path = std::string("dir") + sep + "file.txt";
    EXPECT_EQ(normalize_path(join_path("dir/", "file.txt")), normalize_path(expected_path));
    EXPECT_EQ(normalize_path(join_path("dir", "/file.txt")), normalize_path(expected_path));
}

TEST(YoloAppUnitTests, GetBasename) {
    EXPECT_EQ(get_basename("path/to/file.txt"), "file.txt");
    EXPECT_EQ(get_basename("C:\\Users\\Test\\video.mp4"), "video.mp4");
    EXPECT_EQ(get_basename("file.txt"), "file.txt");
    EXPECT_EQ(get_basename(""), "");
    EXPECT_EQ(get_basename("path/to/"), "");
    EXPECT_EQ(get_basename("path/to/."), ".");
    EXPECT_EQ(get_basename("path/to/.."), "..");
}

TEST(YoloAppUnitTests, LogCurrentTime) {
    // Test with nullptr, should not crash
    log_current_time(nullptr);
    SUCCEED(); // If we got here, it didn't crash.
}

TEST_F(FilesystemTest, ExpandWildcards) {
    // Create some dummy files
    std::ofstream(join_path(test_dir, "test1.txt")).close();
    std::ofstream(join_path(test_dir, "test2.txt")).close();
    std::ofstream(join_path(test_dir, "another.dat")).close();

    // Test wildcard '*'
    std::string pattern1 = join_path(test_dir, "*.txt");
    std::vector<std::string> files1 = expand_wildcards(pattern1);
    ASSERT_EQ(files1.size(), 2);
    // Sort results for consistent testing
    std::sort(files1.begin(), files1.end());
    EXPECT_EQ(files1[0], join_path(test_dir, "test1.txt"));
    EXPECT_EQ(files1[1], join_path(test_dir, "test2.txt"));

    // Test wildcard '?'
    std::string pattern2 = join_path(test_dir, "test?.txt");
    std::vector<std::string> files2 = expand_wildcards(pattern2);
    ASSERT_EQ(files2.size(), 2);

    // Test no wildcard
    std::string pattern3 = join_path(test_dir, "another.dat");
    std::vector<std::string> files3 = expand_wildcards(pattern3);
    ASSERT_EQ(files3.size(), 1);
    EXPECT_EQ(files3[0], pattern3);

    // Test no match
    std::string pattern4 = join_path(test_dir, "*.mp3");
    std::vector<std::string> files4 = expand_wildcards(pattern4);
    EXPECT_TRUE(files4.empty());

    // Test path with spaces
    std::string spaced_dir = join_path(test_dir, "spaced dir");
    std::filesystem::create_directory(spaced_dir);
    std::ofstream(join_path(spaced_dir, "file.txt")).close();
    std::string pattern5 = join_path(spaced_dir, "*.txt");
    std::vector<std::string> files5 = expand_wildcards(pattern5);
    ASSERT_EQ(files5.size(), 1);
    EXPECT_EQ(files5[0], join_path(spaced_dir, "file.txt"));

    std::filesystem::remove_all(spaced_dir);
}

TEST(YoloAppUnitTests, PromptForValueString) {
    // Redirect cin
    std::streambuf* orig_cin = std::cin.rdbuf();
    std::istringstream input("Test Input\n");
    std::cin.rdbuf(input.rdbuf());

    // Redirect cout
    std::streambuf* orig_cout = std::cout.rdbuf();
    std::ostringstream output;
    std::cout.rdbuf(output.rdbuf());

    std::string value;
    prompt_for_value("Enter string: ", value);

    // Restore cin and cout
    std::cin.rdbuf(orig_cin);
    std::cout.rdbuf(orig_cout);

    EXPECT_EQ(value, "Test Input");
    EXPECT_EQ(output.str(), "Enter string: ");
}

TEST(YoloAppUnitTests, PromptForValueInt) {
    // Redirect cin with invalid then valid input
    std::streambuf* orig_cin = std::cin.rdbuf();
    std::istringstream input("bad\n123\n");
    std::cin.rdbuf(input.rdbuf());

    // Redirect cout
    std::streambuf* orig_cout = std::cout.rdbuf();
    std::ostringstream output;
    std::cout.rdbuf(output.rdbuf());

    int value = 0;
    prompt_for_value("Enter int: ", value);

    // Restore cin and cout
    std::cin.rdbuf(orig_cin);
    std::cout.rdbuf(orig_cout);

    EXPECT_EQ(value, 123);
    EXPECT_EQ(output.str(), "Enter int: Invalid input. Please try again.\nEnter int: ");
}

TEST(YoloAppUnitTests, PromptForValueFloat) {
    // Redirect cin with invalid then valid input
    std::streambuf* orig_cin = std::cin.rdbuf();
    std::istringstream input("not-a-float\n42.5\n");
    std::cin.rdbuf(input.rdbuf());

    // Redirect cout
    std::streambuf* orig_cout = std::cout.rdbuf();
    std::ostringstream output;
    std::cout.rdbuf(output.rdbuf());

    float value = 0.0f;
    prompt_for_value("Enter float: ", value);

    // Restore cin and cout
    std::cin.rdbuf(orig_cin);
    std::cout.rdbuf(orig_cout);

    EXPECT_FLOAT_EQ(value, 42.5f);
    EXPECT_EQ(output.str(), "Enter float: Invalid input. Please try again.\nEnter float: ");
}

TEST(YoloAppUnitTests, PrintHelp) {
    // Redirect cout
    std::streambuf* orig_cout = std::cout.rdbuf();
    std::ostringstream output;
    std::cout.rdbuf(output.rdbuf());

    print_help("test_app");

    // Restore cout
    std::cout.rdbuf(orig_cout);

    std::string help_text = output.str();
    EXPECT_NE(help_text.find("Usage: test_app"), std::string::npos);
    EXPECT_NE(help_text.find("--remix"), std::string::npos);
    EXPECT_NE(help_text.find("--help"), std::string::npos);
}

TEST_F(FilesystemTest, GetAudioChannelCount) {
    // This test requires ffmpeg/ffprobe to be in the system PATH.
    std::string audio_file = join_path(test_dir, "stereo.mp3");
    std::string mono_file = join_path(test_dir, "mono.wav");
    std::string video_no_audio_file = join_path(test_dir, "no_audio.mp4");
    std::string non_existent_file = join_path(test_dir, "fake.mp3");

    // Create a dummy stereo audio file with ffmpeg
    std::string create_audio_cmd = std::string(FFMPEG_PATH) + " -f lavfi -i \"sine=d=0.1,pan=stereo|c0=c0|c1=c1\" -y \"" + audio_file + "\" > NUL 2>&1";
    int stereo_result = system(create_audio_cmd.c_str());
    ASSERT_EQ(stereo_result, 0) << "ffmpeg command to create stereo audio file failed. Ensure ffmpeg is in your PATH.";

    // Create a dummy mono audio file
    std::string create_mono_cmd = std::string(FFMPEG_PATH) + " -f lavfi -i \"sine=d=0.1\" -y \"" + mono_file + "\" > NUL 2>&1";
    int mono_result = system(create_mono_cmd.c_str());
    ASSERT_EQ(mono_result, 0) << "ffmpeg command to create mono audio file failed.";

    // Create a dummy video file with no audio
    std::string create_video_cmd = std::string(FFMPEG_PATH) + " -f lavfi -i \"testsrc=d=0.1:s=10x10\" -an -y \"" + video_no_audio_file + "\" > NUL 2>&1";
    int video_result = system(create_video_cmd.c_str());
    ASSERT_EQ(video_result, 0) << "ffmpeg command to create video file failed. Ensure ffmpeg is in your PATH.";

    FILE* log_file = fopen("test_log.txt", "w");
    ASSERT_TRUE(log_file != nullptr);

    EXPECT_EQ(get_audio_channel_count(audio_file, log_file), 2);
    EXPECT_EQ(get_audio_channel_count(mono_file, log_file), 1);
    EXPECT_EQ(get_audio_channel_count(video_no_audio_file, log_file), 2); // Expect fallback value
    EXPECT_EQ(get_audio_channel_count(non_existent_file, log_file), 2);

    fclose(log_file);
}

TEST_F(FilesystemTest, HasVideoStream) {
    std::string video_file = join_path(test_dir, "video.mp4");
    std::string audio_file = join_path(test_dir, "audio.mp3");
    std::string non_existent_file = join_path(test_dir, "fake.mp4");

    system((std::string(FFMPEG_PATH) + " -f lavfi -i \"testsrc=d=0.1:s=10x10\" -y \"" + video_file + "\" > NUL 2>&1").c_str());
    system((std::string(FFMPEG_PATH) + " -f lavfi -i \"sine=d=0.1\" -y \"" + audio_file + "\" > NUL 2>&1").c_str());

    YoloConfig config;
    FILE* log_file = fopen("test_log.txt", "w");

    EXPECT_TRUE(has_video_stream(video_file, &config, log_file));
    EXPECT_FALSE(has_video_stream(audio_file, &config, log_file));
    EXPECT_FALSE(has_video_stream(non_existent_file, &config, log_file));

    // Test caching: call again, should be faster and return same result
    EXPECT_TRUE(has_video_stream(video_file, &config, log_file));
    EXPECT_EQ(config.video_stream_cache.size(), 3); // video, audio, and fake file are now in cache

    fclose(log_file);
}