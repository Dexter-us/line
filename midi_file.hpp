#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace line::midi {

using PatternValue = std::pair<std::uint8_t, std::uint8_t>;
using Pattern = std::vector<std::vector<std::vector<PatternValue>>>;

struct PatternStep {
  std::uint64_t startTick = 0;
  std::uint64_t endTick = 0;
  std::vector<PatternValue> values;
};

struct Event {
  std::uint64_t tick = 0;
  std::uint8_t status = 0;
  std::uint8_t data1 = 0;
  std::uint8_t data2 = 0;
};

enum class Mode { notes, controlChange };

std::vector<PatternStep> makeSteps(const Pattern& pattern, double phraseBeats,
                                   std::uint16_t ticksPerQuarterNote = 480);

std::vector<Event> makeEvents(const std::vector<PatternStep>& steps, Mode mode,
                              std::uint8_t channel, std::uint8_t controller,
                              bool muted);

bool durationSecondsToTicks(double seconds, double bpm, std::uint64_t& ticks,
                            std::string& error);

bool repeatEventsUntil(const std::vector<Event>& phraseEvents,
                       std::uint64_t phraseDurationTicks,
                       std::uint64_t targetDurationTicks,
                       std::vector<Event>& repeatedEvents, std::string& error);

bool writeStandardMidiFile(const std::string& path, double bpm,
                           std::vector<Event> events, std::string& error,
                           std::uint64_t endTick = 0);

}  // namespace line::midi
