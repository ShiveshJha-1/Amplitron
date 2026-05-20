#include "test_framework.h"
#include "preset_json.h"
#include "preset_manager.h"
#include <string>

using namespace Amplitron;

// Test 1: Serialised JSON is non-empty for a valid preset
TEST(ClipboardPresetTest, SerialiseReturnsNonEmptyString) {
    PresetData pm;
    pm.name = "Test Preset";
    pm.description = "Test description";
    pm.input_gain = 0.5f;
    pm.output_gain = 0.5f;
    std::string json = to_json_ext(pm);
    ASSERT_FALSE(json.empty());
}

// Test 2: Serialised JSON is valid JSON (contains { and })
TEST(ClipboardPresetTest, SerialiseReturnsValidJson) {
    PresetData pm;
    pm.name = "Test Preset";
    std::string json = to_json_ext(pm);
    ASSERT_NE(json.find('{'), std::string::npos);
    ASSERT_NE(json.find('}'), std::string::npos);
}

// Test 3: Round-trip — serialise then deserialise gives same preset
TEST(ClipboardPresetTest, RoundTripRestoresSamePreset) {
    PresetData pm;
    pm.name = "Test Preset";
    pm.description = "Test description";
    pm.input_gain = 0.5f;
    pm.output_gain = 0.5f;
    
    std::string json = to_json_ext(pm);

    PresetData pm2;
    bool loaded = from_json_ext(json, pm2);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(to_json_ext(pm), to_json_ext(pm2));
}

// Test 4: Empty/invalid JSON does not crash
TEST(ClipboardPresetTest, LoadInvalidJsonReturnsFalse) {
    PresetData pm;
    bool loaded = from_json_ext("not valid json {{{{", pm);
    ASSERT_FALSE(loaded);
}
