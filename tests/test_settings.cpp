#include <cmath>
#include <filesystem>
#include <fstream>

#include "Settings.h"
#include "Test.h"

namespace fs = std::filesystem;

TEST(settings_roundtrip) {
    const fs::path file = fs::temp_directory_path() / "gp_settings_test" / "settings.ini";
    Settings s;
    s.language = "hi";
    s.particleCount = 23000;
    s.quality = 3;
    s.trails = 0.3f;
    s.lensing = false;
    s.cinematic = false;
    s.gravity = 1.7f;
    s.bloom = 0.5f;
    s.particleSize = 2.0f;
    s.palette = 2;
    s.panelVisible = false;
    s.helpSeen = true;
    CHECK(s.save(file));

    Settings loaded;
    CHECK(loaded.load(file));
    CHECK_EQ(loaded.language, std::string("hi"));
    CHECK_EQ(loaded.particleCount, 23000);
    CHECK(std::abs(loaded.gravity - 1.7f) < 1e-4f);
    CHECK(std::abs(loaded.bloom - 0.5f) < 1e-4f);
    CHECK(std::abs(loaded.particleSize - 2.0f) < 1e-4f);
    CHECK_EQ(loaded.palette, 2);
    CHECK_EQ(loaded.quality, 3);
    CHECK(std::abs(loaded.trails - 0.3f) < 1e-4f);
    CHECK(!loaded.lensing);
    CHECK(!loaded.cinematic);
    CHECK(!loaded.panelVisible);
    CHECK(loaded.helpSeen);
    std::error_code ec;
    fs::remove_all(file.parent_path(), ec);
}

TEST(settings_clamps_and_ignores_garbage) {
    const fs::path file = fs::temp_directory_path() / "gp_settings_garbage.ini";
    std::ofstream(file) << "particles = 999999\ngravity = -5\nbloom = abc\npalette = 7\n"
                           "garbage line\n= no key\nparticle_size = 0.01\n";
    Settings s;
    CHECK(s.load(file));
    CHECK_EQ(s.particleCount, Settings::kMaxParticles);
    CHECK(std::abs(s.gravity - 0.1f) < 1e-4f);
    CHECK(std::abs(s.bloom - Settings().bloom) < 1e-4f);  // нечисловое значение — остаётся по умолчанию
    CHECK_EQ(s.palette, 3);
    CHECK(std::abs(s.particleSize - 0.4f) < 1e-4f);
    std::error_code ec;
    fs::remove(file, ec);
}

TEST(settings_missing_file_keeps_defaults) {
    Settings s;
    CHECK(!s.load(fs::temp_directory_path() / "gp_no_such_dir" / "settings.ini"));
    CHECK_EQ(s.particleCount, Settings::kQuality[Settings().quality].particles);
    CHECK(s.language.empty());
    CHECK(s.lensing && s.cinematic);
}

TEST(settings_particle_count_snaps_to_step) {
    Settings s;
    s.particleCount = 12345;
    s.clamp();
    CHECK_EQ(s.particleCount, 12000);
}

TEST(settings_round_particles_two_significant_digits) {
    CHECK_EQ(Settings::roundParticles(437812), 440000);
    CHECK_EQ(Settings::roundParticles(61234), 61000);
    CHECK_EQ(Settings::roundParticles(1000000), 1000000);
    CHECK_EQ(Settings::roundParticles(10), Settings::kMinParticles);
    CHECK_EQ(Settings::roundParticles(5e9), Settings::kMaxParticles);
}

TEST(settings_quality_presets) {
    Settings s;
    for (int q = 0; q < Settings::kQualityCount; ++q) {
        s.applyQuality(q);
        CHECK_EQ(s.quality, q);
        CHECK_EQ(s.particleCount, Settings::kQuality[q].particles);
        if (q > 0) CHECK(Settings::kQuality[q].particles > Settings::kQuality[q - 1].particles);
    }
    s.applyQuality(99);
    CHECK_EQ(s.quality, Settings::kQualityCount - 1);
}
