#include "line_processor.hpp"

#include "line_cids.hpp"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Steinberg::Vst {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kGateRatio = 0.88;
constexpr std::size_t kMaxSavedPattern = 255;

}  // namespace

LineProcessor::LineProcessor() {
  const auto parsed = line::parsePattern(patternText_);
  if (parsed)
    pattern_ = std::make_shared<const line::Pattern>(parsed.pattern);
}

tresult PLUGIN_API LineProcessor::initialize(FUnknown* context) {
  const auto result = AudioEffect::initialize(context);
  if (result != kResultOk)
    return result;

  setControllerClass(LineControllerUID);
  addEventInput(STR16("MIDI Input"), 16, kMain, BusInfo::kDefaultActive);
  addAudioOutput(STR16("Stereo Output"), SpeakerArr::kStereo, kMain,
                 BusInfo::kDefaultActive);
  addEventOutput(STR16("Pattern MIDI Output"), 16, kMain,
                 BusInfo::kDefaultActive);
  return kResultOk;
}

tresult PLUGIN_API LineProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
  return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API LineProcessor::setupProcessing(ProcessSetup& setup) {
  if (setup.sampleRate <= 0.0)
    return kInvalidArgument;
  sampleRate_ = setup.sampleRate;
  return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API LineProcessor::receiveText(const char8* text) {
  if (text == nullptr)
    return kInvalidArgument;
  if (std::strcmp(text, "PLAY") == 0) {
    enabled_.store(true, std::memory_order_release);
    return kResultOk;
  }
  if (std::strcmp(text, "STOP") == 0) {
    enabled_.store(false, std::memory_order_release);
    return kResultOk;
  }
  if (std::strlen(text) > kMaxSavedPattern)
    return kInvalidArgument;

  const auto parsed = line::parsePattern(text);
  if (!parsed) {
    sendTextMessage(("ERR: " + parsed.error).c_str());
    return kResultFalse;
  }

  patternText_ = text;
  std::atomic_store_explicit(
      &pattern_,
      std::make_shared<const line::Pattern>(parsed.pattern),
      std::memory_order_release);
  sendTextMessage("Pattern accepted");
  return kResultOk;
}

void LineProcessor::addNoteEvent(IEventList* events, Event::EventTypes type,
                                 const line::Note& note, int32 sampleOffset,
                                 double beat) {
  if (!events)
    return;

  Event event{};
  event.busIndex = 0;
  event.sampleOffset = sampleOffset;
  event.ppqPosition = beat;
  event.flags = Event::kIsLive;
  event.type = type;
  if (type == Event::kNoteOnEvent) {
    event.noteOn.channel = 0;
    event.noteOn.pitch = note.pitch;
    event.noteOn.velocity = static_cast<float>(note.velocity) / 127.0f;
    event.noteOn.length = 0;
    event.noteOn.tuning = 0.0f;
    event.noteOn.noteId = -1;
  } else {
    event.noteOff.channel = 0;
    event.noteOff.pitch = note.pitch;
    event.noteOff.velocity = 0.0f;
    event.noteOff.tuning = 0.0f;
    event.noteOff.noteId = -1;
  }
  events->addEvent(event);
}

void LineProcessor::releaseVoices(IEventList* events, int32 sampleOffset,
                                  double beat) {
  for (auto& voice : voices_) {
    if (!voice.active || voice.releasing)
      continue;
    addNoteEvent(events, Event::kNoteOffEvent,
                 {voice.pitch, static_cast<std::uint8_t>(100)}, sampleOffset,
                 beat);
    voice.releasing = true;
    voice.releasePerSample = std::max(
        voice.level / static_cast<float>(sampleRate_ * 0.035), 0.00001f);
  }
}

void LineProcessor::startStep(const line::Step& step, IEventList* events,
                              int32 sampleOffset, double beat) {
  releaseVoices(events, sampleOffset, beat);
  for (std::size_t noteIndex = 0; noteIndex < step.noteCount; ++noteIndex) {
    const auto& note = step.notes[noteIndex];
    auto voice = std::find_if(
        voices_.begin(), voices_.end(),
        [](const Voice& item) { return !item.active || item.releasing; });
    if (voice == voices_.end())
      voice = voices_.begin();

    voice->pitch = note.pitch;
    voice->phase = 0.0;
    const double frequency =
        440.0 * std::pow(2.0, (static_cast<int>(note.pitch) - 69) / 12.0);
    voice->phaseIncrement = kTwoPi * frequency / sampleRate_;
    voice->level = static_cast<float>(note.velocity) / 127.0f;
    voice->releasePerSample = 0.0f;
    voice->ageSamples = 0;
    voice->active = true;
    voice->releasing = false;
    addNoteEvent(events, Event::kNoteOnEvent, note, sampleOffset, beat);
  }
  gateClosed_ = step.noteCount == 0;
}

tresult PLUGIN_API LineProcessor::process(ProcessData& data) {
  if (data.numSamples <= 0 || data.numOutputs < 1 || data.outputs == nullptr)
    return kResultTrue;

  auto& output = data.outputs[0];
  for (int32 channel = 0; channel < output.numChannels; ++channel) {
    if (output.channelBuffers32 && output.channelBuffers32[channel])
      std::fill_n(output.channelBuffers32[channel], data.numSamples, 0.0f);
  }

  auto pattern = std::atomic_load_explicit(&pattern_, std::memory_order_acquire);
  if (!enabled_.load(std::memory_order_acquire) || !pattern ||
      pattern->stepCount == 0 ||
      output.channelBuffers32 == nullptr) {
    releaseVoices(data.outputEvents, 0, internalBeat_);
    lastStepIndex_ = line::kMaxSteps;
    lastStepStartBeat_ = -1.0;
    return kResultTrue;
  }

  const auto* context = data.processContext;
  if (context && (context->state & ProcessContext::kPlaying) == 0) {
    releaseVoices(data.outputEvents, 0, internalBeat_);
    lastStepIndex_ = line::kMaxSteps;
    lastStepStartBeat_ = -1.0;
    internalBeat_ = 0.0;
    return kResultTrue;
  }

  double tempo = pattern->bpm;
  if (context && (context->state & ProcessContext::kTempoValid) &&
      std::isfinite(context->tempo) && context->tempo >= 20.0 &&
      context->tempo <= 300.0)
    tempo = context->tempo;
  const double beatPerSample = tempo / (60.0 * sampleRate_);

  double startBeat = internalBeat_;
  if (context && (context->state & ProcessContext::kProjectTimeMusicValid) &&
      std::isfinite(context->projectTimeMusic))
    startBeat = context->projectTimeMusic;

  const double patternLength = pattern->lengthInBeats();
  if (patternLength <= 0.0)
    return kResultTrue;

  double withinPattern = std::fmod(startBeat, patternLength);
  if (withinPattern < 0.0)
    withinPattern += patternLength;
  double stepStartBeat = startBeat - withinPattern;
  std::size_t stepIndex = 0;
  while (stepIndex + 1 < pattern->stepCount &&
         withinPattern >= pattern->steps[stepIndex].beats) {
    withinPattern -= pattern->steps[stepIndex].beats;
    stepStartBeat += pattern->steps[stepIndex].beats;
    ++stepIndex;
  }

  const bool newPosition =
      lastStepIndex_ != stepIndex ||
      std::abs(lastStepStartBeat_ - stepStartBeat) > 0.0000001;
  if (newPosition) {
    startStep(pattern->steps[stepIndex], data.outputEvents, 0, startBeat);
    lastStepStartBeat_ = stepStartBeat;
    lastStepIndex_ = stepIndex;
    gateClosed_ = false;
  }

  for (int32 sample = 0; sample < data.numSamples; ++sample) {
    const double beat = startBeat + sample * beatPerSample;
    while (beat + 1.0e-10 >=
           stepStartBeat + pattern->steps[stepIndex].beats) {
      stepStartBeat += pattern->steps[stepIndex].beats;
      ++stepIndex;
      if (stepIndex == pattern->stepCount) {
        stepIndex = 0;
      }
      startStep(pattern->steps[stepIndex], data.outputEvents, sample, beat);
      lastStepStartBeat_ = stepStartBeat;
      lastStepIndex_ = stepIndex;
      gateClosed_ = false;
    }

    const auto& step = pattern->steps[stepIndex];
    if (!gateClosed_ && step.noteCount > 0 &&
        beat >= stepStartBeat + step.beats * kGateRatio) {
      releaseVoices(data.outputEvents, sample, beat);
      gateClosed_ = true;
    }

    float sampleValue = 0.0f;
    for (auto& voice : voices_) {
      if (!voice.active)
        continue;
      if (voice.releasing) {
        voice.level -= voice.releasePerSample;
        if (voice.level <= 0.0001f) {
          voice.level = 0.0f;
          voice.active = false;
          voice.releasing = false;
          continue;
        }
      }
      const float attack = std::min(
          1.0f, static_cast<float>(voice.ageSamples / (sampleRate_ * 0.008)));
      sampleValue += static_cast<float>(std::sin(voice.phase)) * voice.level *
                     attack * 0.16f;
      ++voice.ageSamples;
      voice.phase += voice.phaseIncrement;
      if (voice.phase >= kTwoPi)
        voice.phase -= kTwoPi;
    }

    for (int32 channel = 0; channel < output.numChannels; ++channel) {
      if (output.channelBuffers32[channel])
        output.channelBuffers32[channel][sample] =
            std::max(-1.0f, std::min(1.0f, sampleValue));
    }
  }

  internalBeat_ = startBeat + data.numSamples * beatPerSample;
  return kResultTrue;
}

bool LineProcessor::writeText(IBStream* state, const std::string& text) {
  if (!state || text.size() > kMaxSavedPattern)
    return false;
  const int32 length = static_cast<int32>(text.size());
  int32 written = 0;
  if (state->write(&length, sizeof(length), &written) != kResultOk ||
      written != sizeof(length))
    return false;
  if (length == 0)
    return true;
  written = 0;
  return state->write(text.data(), length, &written) == kResultOk &&
         written == length;
}

bool LineProcessor::readText(IBStream* state, std::string& text) {
  if (!state)
    return false;
  int32 length = 0;
  int32 read = 0;
  if (state->read(&length, sizeof(length), &read) != kResultOk ||
      read != sizeof(length) || length < 0 ||
      length > static_cast<int32>(kMaxSavedPattern))
    return false;
  text.assign(static_cast<std::size_t>(length), '\0');
  if (length == 0)
    return true;
  read = 0;
  return state->read(text.data(), length, &read) == kResultOk && read == length;
}

tresult PLUGIN_API LineProcessor::setState(IBStream* state) {
  std::string restored;
  if (!readText(state, restored))
    return kResultFalse;
  return receiveText(restored.c_str());
}

tresult PLUGIN_API LineProcessor::getState(IBStream* state) {
  return writeText(state, patternText_) ? kResultOk : kResultFalse;
}

}  // namespace Steinberg::Vst
