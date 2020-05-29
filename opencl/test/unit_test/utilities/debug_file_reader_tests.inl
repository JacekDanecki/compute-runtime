/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/debug_file_reader.h"

#include "test.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>

using namespace NEO;

class TestSettingsFileReader : public SettingsFileReader {
  public:
    using SettingsFileReader::parseStream;

    TestSettingsFileReader(const char *filePath = nullptr) : SettingsFileReader(filePath) {
    }

    ~TestSettingsFileReader() override {
    }

    bool hasSetting(const char *settingName) {
        std::map<std::string, std::string>::iterator it = settingStringMap.find(std::string(settingName));
        return (it != settingStringMap.end());
    }

    size_t getStringSettingsCount() {
        return settingStringMap.size();
    }

    static const char *testPath;
    static const char *stringTestPath;
};

const char *TestSettingsFileReader::testPath = "./test_files/igdrcl.config";
const char *TestSettingsFileReader::stringTestPath = "./test_files/igdrcl_string.config";

TEST(SettingsFileReader, CreateFileReaderWithoutFile) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);

    // if settings file exists, remove it
    if (settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }

    // Use current location for file read
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);

    EXPECT_EQ(0u, reader->getStringSettingsCount());
}

TEST(SettingsFileReader, GetStringSettingFromFile) {
    // Use test settings file
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::stringTestPath);
    ASSERT_NE(nullptr, reader);

    std::string retValue;
    // StringTestKey is defined in file: unit_tests\helpers\test_debug_variables.inl
    std::string returnedStringValue = reader->getSetting("StringTestKey", retValue);

    // "Test Value" is a value that should be read from file defined in stringTestPath member
    EXPECT_STREQ(returnedStringValue.c_str(), "TestValue");

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    {                                                                             \
        dataType defaultData = defaultValue;                                      \
        dataType tempData = reader->getSetting(#variableName, defaultData);       \
        if (tempData == defaultData) {                                            \
            EXPECT_TRUE(true);                                                    \
        }                                                                         \
    }
#include "opencl/test/unit_test/helpers/test_debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

TEST(SettingsFileReader, givenDebugFileSettingInWhichStringIsFollowedByIntegerWhenItIsParsedThenProperValuesAreObtained) {
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::stringTestPath);
    ASSERT_NE(nullptr, reader.get());

    int32_t retValue = 0;
    int32_t returnedIntValue = reader->getSetting("IntTestKey", retValue);

    EXPECT_EQ(1, returnedIntValue);

    std::string retValueString;
    std::string returnedStringValue = reader->getSetting("StringTestKey", retValueString);

    EXPECT_STREQ(returnedStringValue.c_str(), "TestValue");
}

TEST(SettingsFileReader, GetSettingWhenNotInFile) {

    // Use test settings file
    auto reader = std::make_unique<TestSettingsFileReader>(TestSettingsFileReader::testPath);
    ASSERT_NE(nullptr, reader);

    bool defaultBoolValue = false;
    bool returnedBoolValue = reader->getSetting("BoolSettingNotExistingInFile", defaultBoolValue);

    EXPECT_EQ(defaultBoolValue, returnedBoolValue);

    int32_t defaultIntValue = 123;
    int32_t returnedIntValue = reader->getSetting("IntSettingNotExistingInFile", defaultIntValue);

    EXPECT_EQ(defaultIntValue, returnedIntValue);

    std::string defaultStringValue = "ABCD";
    std::string returnedStringValue = reader->getSetting("StringSettingNotExistingInFile", defaultStringValue);

    EXPECT_EQ(defaultStringValue, returnedStringValue);
}

TEST(SettingsFileReader, appSpecificLocation) {
    std::unique_ptr<TestSettingsFileReader> reader(new TestSettingsFileReader(TestSettingsFileReader::testPath));
    std::string appSpecific = "cl_cache_dir";
    EXPECT_EQ(appSpecific, reader->appSpecificLocation(appSpecific));
}

TEST(SettingsFileReader, givenHexNumbersSemiColonSeparatedListInInputStreamWhenParsingThenCorrectStringValueIsStored) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);

    //No settings should be parsed initially
    EXPECT_EQ(0u, reader->getStringSettingsCount());

    std::stringstream inputLineWithSemiColonList("KeyName = 0x1234;0x5555");

    reader->parseStream(inputLineWithSemiColonList);

    std::string defaultStringValue = "FailedToParse";
    std::string returnedStringValue = reader->getSetting("KeyName", defaultStringValue);

    EXPECT_STREQ("0x1234;0x5555", returnedStringValue.c_str());
}

TEST(SettingsFileReader, given64bitKeyValueWhenGetSettingThenValueIsCorrect) {
    auto reader = std::make_unique<TestSettingsFileReader>();
    ASSERT_NE(nullptr, reader);

    EXPECT_EQ(0u, reader->getStringSettingsCount());
    std::stringstream inputLine("Example64BitKey = -18764712120594");
    reader->parseStream(inputLine);

    int64_t defaultValue = 0;
    int64_t returnedValue = reader->getSetting("Example64BitKey", defaultValue);

    EXPECT_EQ(-18764712120594, returnedValue);
}
