/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoPlayer.h"

#include "interface/StreamInfo.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

// Forward declare class not in VideoPlayer.h
class PredicateSubtitlePriority
{
  private:
    std::string m_playedAudioLang;
    std::string m_subLang;
    bool m_isPrefOriginal;
    bool m_isPrefForced;
    bool m_isPrefHearingImp;
    bool m_isSubNone;
    int m_subStream;

  public:
    explicit PredicateSubtitlePriority(const std::string& lang, int stream);
    bool relevant(const SelectionStream& stream) const;
    bool operator()(const SelectionStream& lh, const SelectionStream& rh) const;
};

class TestPredicateSubtitlePriority : public testing::Test
{
public:
  void SetSettings(std::string audioLanguage, std::string subLanguage, bool isHearingImpaired)
  {
    std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
    settings->SetString(CSettings::SETTING_LOCALE_AUDIOLANGUAGE, audioLanguage);
    settings->SetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE, subLanguage);
    settings->SetBool(CSettings::SETTING_ACCESSIBILITY_SUBHEARING, isHearingImpaired);
    // Verify SETTING_LOCALE_AUDIOLANGUAGE is correct before continuing
    std::string audioLangSetting = settings->GetString(CSettings::SETTING_LOCALE_AUDIOLANGUAGE);
    ASSERT_EQ(audioLangSetting, audioLanguage);
    // Verify SETTING_LOCALE_SUBTITLELANGUAGE is correct before continuing
    std::string subLangSetting = settings->GetString(CSettings::SETTING_LOCALE_SUBTITLELANGUAGE);
    ASSERT_EQ(subLangSetting, subLanguage);
    // Verify SETTING_ACCESSIBILITY_SUBHEARING is correct before continuing
    bool subHearingImpSetting = settings->GetBool(CSettings::SETTING_ACCESSIBILITY_SUBHEARING);
    ASSERT_EQ(subHearingImpSetting, isHearingImpaired);
  }
};

TEST_F(TestPredicateSubtitlePriority, SameStreamRelevant)
{
  // Subtitles are always relevant if the same stream used when constructing PredicateSubtitlePriority

  SetSettings("eng", "none", false);
  PredicateSubtitlePriority subPriority("eng", 0);

  // SelectionStream default type_index = 0
  SelectionStream stream;
  stream.language = "eng";
  stream.flags = FLAG_NONE;
  
  EXPECT_TRUE(subPriority.relevant(stream));
}

// Subtitles are never relevant when user subtitle setting is "none" (stream is different)
TEST_F(TestPredicateSubtitlePriority, TestNoneSettingNeverRelevant)
{
  SetSettings("eng", "none", false);
  PredicateSubtitlePriority subPriority("", 1);

  SelectionStream stream;
  stream.type_index = 0;
  stream.language = "eng";
  stream.flags = FLAG_NONE;

  EXPECT_FALSE(subPriority.relevant(stream));
}

// External subtitles with unknown language are always relevant
TEST_F(TestPredicateSubtitlePriority, TestExternalUnknownRelevant)
{
  SetSettings("eng", "eng", false);
  PredicateSubtitlePriority subPriority("", 1);

  SelectionStream streamSourceDemuxSubNoLang;
  streamSourceDemuxSubNoLang.flags = FLAG_NONE;
  streamSourceDemuxSubNoLang.source = StreamSource::STREAM_SOURCE_DEMUX_SUB;
  EXPECT_TRUE(subPriority.relevant(streamSourceDemuxSubNoLang));

  SelectionStream streamSourceTextNoLang;
  streamSourceTextNoLang.flags = FLAG_NONE;
  streamSourceTextNoLang.source = StreamSource::STREAM_SOURCE_TEXT;
  EXPECT_TRUE(subPriority.relevant(streamSourceTextNoLang));

  SelectionStream streamSourceDemuxSubUndLang;
  streamSourceDemuxSubUndLang.language = "und";
  streamSourceDemuxSubUndLang.flags = FLAG_NONE;
  streamSourceDemuxSubUndLang.source = StreamSource::STREAM_SOURCE_DEMUX_SUB;
  EXPECT_TRUE(subPriority.relevant(streamSourceDemuxSubUndLang));

  SelectionStream streamSourceTextUndLang;
  streamSourceTextUndLang.language = "und";
  streamSourceTextUndLang.flags = FLAG_NONE;
  streamSourceTextUndLang.source = StreamSource::STREAM_SOURCE_TEXT;
  EXPECT_TRUE(subPriority.relevant(streamSourceTextUndLang));
}

// CC subtitles with unknown language are always relevant with hearing impaired setting
TEST_F(TestPredicateSubtitlePriority, TestHearingImpairedSettingCCUnknownRelevant)
{
  SetSettings("eng", "eng", true);
  PredicateSubtitlePriority subPriority("", 1);

  SelectionStream streamNoLang;
  streamNoLang.flags = FLAG_HEARING_IMPAIRED;
  streamNoLang.source = StreamSource::STREAM_SOURCE_VIDEOMUX;
  EXPECT_TRUE(subPriority.relevant(streamNoLang));

  SelectionStream streamUndLang;
  streamUndLang.language = "und";
  streamUndLang.flags = FLAG_HEARING_IMPAIRED;
  streamUndLang.source = StreamSource::STREAM_SOURCE_VIDEOMUX;
  EXPECT_TRUE(subPriority.relevant(streamUndLang));
}

struct RelevantTestCase
{
  std::string subLangSetting;
  bool hearingImpSetting;
  std::string audioLangSetting; // Only used when subLangSetting is not a language
  std::string streamLang;
  StreamFlags streamFlags;
  bool isRelevant;
};

class TestSubtitleRelevant : public testing::WithParamInterface<RelevantTestCase>,
                                    public TestPredicateSubtitlePriority
{
};

const auto relevant_cases = std::array
{
  RelevantTestCase{"original", true, "eng", "eng", static_cast<StreamFlags>(FLAG_HEARING_IMPAIRED | FLAG_ORIGINAL), true},
  RelevantTestCase{"original", true, "eng", "eng", FLAG_HEARING_IMPAIRED, false}, // Is this desired behavior?
  RelevantTestCase{"original", true, "eng", "eng", FLAG_NONE, false}, // Is this desired behavior?
  RelevantTestCase{"original", true, "eng", "swe", FLAG_HEARING_IMPAIRED, false},
  RelevantTestCase{"original", true, "eng", "eng", FLAG_FORCED, false},
  RelevantTestCase{"forced_only", true, "eng", "eng", static_cast<StreamFlags>(FLAG_HEARING_IMPAIRED | FLAG_ORIGINAL), true},
  RelevantTestCase{"forced_only", true, "eng", "eng", FLAG_HEARING_IMPAIRED, false}, // Is this desired behavior?
  RelevantTestCase{"forced_only", true, "eng", "eng", FLAG_NONE, false}, // Is this desired behavior?
  RelevantTestCase{"forced_only", true, "eng", "swe", FLAG_HEARING_IMPAIRED, false},
  RelevantTestCase{"forced_only", true, "eng", "eng", FLAG_FORCED, false},
  RelevantTestCase{"eng", true, "und", "eng", static_cast<StreamFlags>(FLAG_HEARING_IMPAIRED | FLAG_ORIGINAL), true},
  RelevantTestCase{"eng", true, "und", "eng", FLAG_HEARING_IMPAIRED, false}, // Is this desired behavior?
  RelevantTestCase{"eng", true, "und", "swe", FLAG_HEARING_IMPAIRED, false},
  RelevantTestCase{"swe", true, "und", "eng", static_cast<StreamFlags>(FLAG_HEARING_IMPAIRED | FLAG_ORIGINAL), true}, // Is this desired behavior?
  RelevantTestCase{"swe", true, "und", "swe", FLAG_HEARING_IMPAIRED, false}, // Is this desired behavior?
  RelevantTestCase{"swe", true, "und", "eng", FLAG_HEARING_IMPAIRED, false},
  RelevantTestCase{"original", false, "eng", "eng", FLAG_NONE, false}, // Is this desired behavior?
  RelevantTestCase{"original", false, "eng", "eng", FLAG_ORIGINAL, true},
  RelevantTestCase{"original", false, "eng", "swe", FLAG_ORIGINAL, true},
  RelevantTestCase{"original", false, "eng", "eng", static_cast<StreamFlags>(FLAG_HEARING_IMPAIRED | FLAG_ORIGINAL), true},
  RelevantTestCase{"original", false, "eng", "swe", FLAG_NONE, false},
  RelevantTestCase{"original", false, "eng", "eng", FLAG_FORCED, false},
  RelevantTestCase{"original", false, "eng", "eng", FLAG_HEARING_IMPAIRED, false},
  RelevantTestCase{"forced_only", false, "eng", "eng", FLAG_FORCED, false}, // Is this desired behavior?
  RelevantTestCase{"forced_only", false, "eng", "swe", FLAG_FORCED, false},
  RelevantTestCase{"forced_only", false, "eng", "eng", FLAG_NONE, false},
  RelevantTestCase{"forced_only", false, "eng", "eng", FLAG_ORIGINAL, false},
  RelevantTestCase{"forced_only", false, "eng", "eng", FLAG_HEARING_IMPAIRED, false},
  RelevantTestCase{"eng", false, "und", "eng", FLAG_NONE, false}, // Is this desired behavior?
  RelevantTestCase{"eng", false, "und", "eng", FLAG_ORIGINAL, false}, // Is this desired behavior?
  RelevantTestCase{"eng", false, "und", "eng", FLAG_FORCED, false},
  RelevantTestCase{"eng", false, "und", "eng", FLAG_HEARING_IMPAIRED, false}, // Is this desired behavior?
  RelevantTestCase{"eng", false, "und", "swe", FLAG_NONE, false},
  RelevantTestCase{"eng", false, "und", "swe", FLAG_ORIGINAL, false},
  RelevantTestCase{"swe", false, "und", "swe", FLAG_NONE, false}, // Is this desired behavior?
  RelevantTestCase{"swe", false, "und", "swe", FLAG_ORIGINAL, false}, // Is this desired behavior?
  RelevantTestCase{"swe", false, "und", "swe", FLAG_FORCED, false},
  RelevantTestCase{"swe", false, "und", "swe", FLAG_HEARING_IMPAIRED, false}, // Is this desired behavior?
  RelevantTestCase{"swe", false, "und", "eng", FLAG_NONE, false},
  RelevantTestCase{"swe", false, "und", "eng", FLAG_ORIGINAL, false}
};

INSTANTIATE_TEST_SUITE_P(RelevantCases, TestSubtitleRelevant, testing::ValuesIn(relevant_cases),
[](const testing::TestParamInfo<TestSubtitleRelevant::ParamType>& info) {
  RelevantTestCase testCase = info.param;
  std::boolalpha;
  std::string name = 
      testCase.subLangSetting +
      std::to_string(testCase.hearingImpSetting) +
      testCase.audioLangSetting +
      testCase.streamLang +
      std::to_string(testCase.streamFlags);
  return name;
});

TEST_P(TestSubtitleRelevant, StreamRelevantToSettings)
{
  RelevantTestCase testCase = GetParam();
  
  SetSettings(testCase.audioLangSetting, testCase.subLangSetting, testCase.hearingImpSetting);
  PredicateSubtitlePriority subPriority("", 1);

  SelectionStream stream{ .language{testCase.streamLang}, .flags{testCase.streamFlags} };
  EXPECT_EQ(subPriority.relevant(stream), testCase.isRelevant);
}
