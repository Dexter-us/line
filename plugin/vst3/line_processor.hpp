#pragma once

#include "line_pattern.hpp"
#include "pluginterfaces/vst/ivstevents.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>

namespace Steinberg::Vst {

class LineProcessor final : public AudioEffect {
 public:
  LineProcessor();

  static FUnknown* createInstance(void*) {
    return static_cast<IAudioProcessor*>(new LineProcessor());
  }

  tresult PLUGIN_API initialize(FUnknown* context) override;
  tresult PLUGIN_API setState(IBStream* state) override;
  tresult PLUGIN_API getState(IBStream* state) override;
  tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) override;
  tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override;
  tresult PLUGIN_API process(ProcessData& data) override;
  tresult PLUGIN_API receiveText(const char8* text) override;

 private:
  struct Voice {
    std::uint8_t pitch = 0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float level = 0.0f;
    float releasePerSample = 0.0f;
    std::uint32_t ageSamples = 0;
    std::int16_t channel = 0;
    std::int32_t noteId = -1;
    bool active = false;
    bool releasing = false;
    bool patternGenerated = false;
  };

  void releasePatternVoices(IEventList* events, int32 sampleOffset, double beat);
  void startStep(const line::Step& step, IEventList* events, int32 sampleOffset,
                 double beat);
  void processInputEvent(const Event& event);
  void releaseVoice(Voice& voice, IEventList* events, int32 sampleOffset,
                    double beat);
  void addNoteEvent(IEventList* events, Event::EventTypes type,
                    const line::Note& note, int32 sampleOffset, double beat);
  static bool writeText(IBStream* state, const std::string& text);
  static bool readText(IBStream* state, std::string& text);

  std::shared_ptr<const line::Pattern> pattern_;
  std::string patternText_ = "c3 e3 g3 c4";
  std::array<Voice, line::kMaxChordNotes * 2> voices_{};
  double internalBeat_ = 0.0;
  double lastStepStartBeat_ = -1.0;
  double sampleRate_ = 44100.0;
  std::size_t lastStepIndex_ = line::kMaxSteps;
  std::atomic<bool> enabled_{true};
  bool gateClosed_ = true;
};

}  // namespace Steinberg::Vst
