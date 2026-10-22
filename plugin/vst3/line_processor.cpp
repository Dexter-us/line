#include "line_processor.hpp"

#include "line_cids.hpp"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
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

void LineProcessor::releaseVoice(Voice& voice, IEventList* events,
                                 int32 sampleOffset, double beat) {
  if (!voice.active || voice.releasing)
    return;
  if (voice.patternGenerated) {
    addNoteEvent(events, Event::kNoteOffEvent,
                 {voice.pitch, static_cast<std::uint8_t>(100)}, sampleOffset,
                 beat);
  }
  voice.releasing = true;
  voice.releasePerSample = std::max(
      voice.level / static_cast<float>(sampleRate_ * 0.035), 0.00001f);
}

void LineProcessor::releasePatternVoices(IEventList* events,
                                         int32 sampleOffset, double beat) {
  for (auto& voice : voices_) {
    if (voice.patternGenerated)
      releaseVoice(voice, events, sampleOffset, beat);
  }
}

void LineProcessor::processInputEvent(const Event& event) {
  if (event.type == Event::kNoteOnEvent && event.noteOn.velocity > 0.0f) {
    const auto& incoming = event.noteOn;
    auto voice = std::find_if(
        voices_.begin(), voices_.end(),
        [](const Voice& item) { return !item.active || item.releasing; });
    if (voice == voices_.end())
      voice = voices_.begin();

    voice->pitch = static_cast<std::uint8_t>(
        std::max(0, std::min(127, static_cast<int>(incoming.pitch))));
    voice->phase = 0.0;
    const double frequency =
        440.0 * std::pow(2.0, (static_cast<int>(voice->pitch) - 69) / 12.0);
    voice->phaseIncrement = kTwoPi * frequency / sampleRate_;
    voice->level = std::max(0.0f, std::min(1.0f, incoming.velocity));
    voice->releasePerSample = 0.0f;
    voice->ageSamples = 0;
    voice->channel = incoming.channel;
    voice->noteId = incoming.noteId;
    voice->active = true;
    voice->releasing = false;
    voice->patternGenerated = false;
    return;
  }

  if (event.type != Event::kNoteOffEvent &&
      !(event.type == Event::kNoteOnEvent && event.noteOn.velocity <= 0.0f))
    return;

  const auto channel = event.type == Event::kNoteOffEvent
                           ? event.noteOff.channel
                           : event.noteOn.channel;
  const auto pitch = event.type == Event::kNoteOffEvent
                         ? event.noteOff.pitch
                         : event.noteOn.pitch;
  const auto noteId = event.type == Event::kNoteOffEvent
                          ? event.noteOff.noteId
                          : event.noteOn.noteId;
  for (auto& voice : voices_) {
    if (!voice.active || voice.releasing || voice.patternGenerated ||
        voice.channel != channel || voice.pitch != pitch)
      continue;
    if (noteId >= 0 && voice.noteId >= 0 && voice.noteId != noteId)
      continue;
    releaseVoice(voice, nullptr, event.sampleOffset, internalBeat_);
  }
}

void LineProcessor::startStep(const line::Step& step, IEventList* events,
                              int32 sampleOffset, double beat) {
  releasePatternVoices(events, sampleOffset, beat);
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
    voice->channel = 0;
    voice->noteId = -1;
    voice->active = true;
    voice->releasing = false;
    voice->patternGenerated = true;
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
  const auto* context = data.processContext;
  const bool hostTransportPlaying =
      context && (context->state & ProcessContext::kPlaying) != 0;
  const bool sequenceEnabled =
      enabled_.load(std::memory_order_acquire) && pattern &&
      pattern->stepCount > 0;
  const bool sequencePlaying = sequenceEnabled;
  if (!sequencePlaying) {
    releasePatternVoices(data.outputEvents, 0, internalBeat_);
    lastStepIndex_ = line::kMaxSteps;
    lastStepStartBeat_ = -1.0;
    if (!hostTransportPlaying)
      internalBeat_ = 0.0;
  }

  double tempo = pattern ? pattern->bpm : 120.0;
  if (hostTransportPlaying && context &&
      (context->state & ProcessContext::kTempoValid) &&
      std::isfinite(context->tempo) && context->tempo >= 20.0 &&
      context->tempo <= 300.0)
    tempo = context->tempo;
  const double beatPerSample = tempo / (60.0 * sampleRate_);

  double startBeat = internalBeat_;
  if (sequencePlaying && hostTransportPlaying && context &&
      (context->state & ProcessContext::kProjectTimeMusicValid) &&
      std::isfinite(context->projectTimeMusic))
    startBeat = context->projectTimeMusic;

  double stepStartBeat = startBeat;
  std::size_t stepIndex = 0;
  bool hasPattern = false;
  if (sequencePlaying) {
    const double patternLength = pattern->lengthInBeats();
    if (patternLength > 0.0) {
      double withinPattern = std::fmod(startBeat, patternLength);
      if (withinPattern < 0.0)
        withinPattern += patternLength;
      stepStartBeat = startBeat - withinPattern;
      while (stepIndex + 1 < pattern->stepCount &&
             withinPattern >= pattern->steps[stepIndex].beats) {
        withinPattern -= pattern->steps[stepIndex].beats;
        stepStartBeat += pattern->steps[stepIndex].beats;
        ++stepIndex;
      }
      hasPattern = true;
    }
  }

  if (hasPattern) {
    const bool newPosition =
        lastStepIndex_ != stepIndex ||
        std::abs(lastStepStartBeat_ - stepStartBeat) > 0.0000001;
    if (newPosition) {
      startStep(pattern->steps[stepIndex], data.outputEvents, 0, startBeat);
      lastStepStartBeat_ = stepStartBeat;
      lastStepIndex_ = stepIndex;
      gateClosed_ = false;
    }
  }

  int32 inputEventIndex = 0;
  Event inputEvent{};
  bool hasInputEvent =
      data.inputEvents && data.inputEvents->getEventCount() > 0 &&
      data.inputEvents->getEvent(inputEventIndex, inputEvent) == kResultOk;

  for (int32 sample = 0; sample < data.numSamples; ++sample) {
    const double beat = startBeat + sample * beatPerSample;
    while (hasInputEvent && inputEvent.sampleOffset <= sample) {
      processInputEvent(inputEvent);
      ++inputEventIndex;
      hasInputEvent =
          inputEventIndex < data.inputEvents->getEventCount() &&
          data.inputEvents->getEvent(inputEventIndex, inputEvent) == kResultOk;
    }

    if (hasPattern) {
      while (beat + 1.0e-10 >=
             stepStartBeat + pattern->steps[stepIndex].beats) {
        stepStartBeat += pattern->steps[stepIndex].beats;
        ++stepIndex;
        if (stepIndex == pattern->stepCount)
          stepIndex = 0;
        startStep(pattern->steps[stepIndex], data.outputEvents, sample, beat);
        lastStepStartBeat_ = stepStartBeat;
        lastStepIndex_ = stepIndex;
        gateClosed_ = false;
      }

      const auto& step = pattern->steps[stepIndex];
      if (!gateClosed_ && step.noteCount > 0 &&
          beat >= stepStartBeat + step.beats * kGateRatio) {
        releasePatternVoices(data.outputEvents, sample, beat);
        gateClosed_ = true;
      }
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

  if (sequencePlaying)
    internalBeat_ = startBeat + data.numSamples * beatPerSample;
  else if (!hostTransportPlaying)
    internalBeat_ = 0.0;
  return kResultTrue;
}

bool LineProcessor::writeText(IBStream* state, const std::string& text) {
  if (!state || text.size() > kMaxSavedPattern)
    return false;
  int32 length = static_cast<int32>(text.size());
  int32 written = 0;
  if (state->write(&length, sizeof(length), &written) != kResultOk ||
      written != sizeof(length))
    return false;
  if (length == 0)
    return true;
  std::string buffer = text;
  written = 0;
  return state->write(buffer.data(), length, &written) == kResultOk &&
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
