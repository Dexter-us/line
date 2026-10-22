#include "midi_file.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <tuple>

namespace line::midi {
namespace {

constexpr std::uint16_t kTicksPerQuarterNote = 480;
constexpr std::uint64_t kMaximumVariableLength = 0x0FFFFFFF;

void appendBigEndian16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void appendBigEndian32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void appendVariableLength(std::vector<std::uint8_t>& bytes,
                          std::uint32_t value) {
  std::uint8_t buffer[4]{};
  std::size_t count = 1;
  buffer[3] = static_cast<std::uint8_t>(value & 0x7F);
  while ((value >>= 7) != 0 && count < 4) {
    buffer[3 - count] =
        static_cast<std::uint8_t>((value & 0x7F) | 0x80);
    ++count;
  }
  const std::size_t first = 4 - count;
  for (std::size_t i = first; i < 4; ++i)
    bytes.push_back(buffer[i]);
}

bool isNoteOff(const Event& event) {
  return (event.status & 0xF0) == 0x80 ||
         ((event.status & 0xF0) == 0x90 && event.data2 == 0);
}

}  // namespace

std::vector<PatternStep> makeSteps(const Pattern& pattern, double phraseBeats,
                                   std::uint16_t ticksPerQuarterNote) {
  std::vector<PatternStep> steps;
  if (pattern.empty() || !std::isfinite(phraseBeats) || phraseBeats <= 0.0 ||
      ticksPerQuarterNote == 0)
    return steps;

  std::uint64_t tick = 0;
  for (const auto& subPhrase : pattern) {
    if (subPhrase.empty())
      continue;

    const double stepBeats =
        phraseBeats / static_cast<double>(pattern.size()) /
        static_cast<double>(subPhrase.size());
    const auto stepTicks = static_cast<std::uint64_t>(
        std::max(1.0, std::round(stepBeats * ticksPerQuarterNote)));
    for (const auto& values : subPhrase) {
      PatternStep step;
      step.startTick = tick;
      step.endTick = tick + std::max<std::uint64_t>(1, stepTicks - 1);
      step.values = values;
      steps.push_back(std::move(step));
      tick += stepTicks;
    }
  }
  return steps;
}

std::vector<Event> makeEvents(const std::vector<PatternStep>& steps, Mode mode,
                              std::uint8_t channel, std::uint8_t controller,
                              bool muted) {
  std::vector<Event> events;
  events.reserve(steps.size() * 4);

  for (const auto& step : steps) {
    for (const auto& value : step.values) {
      if (mode == Mode::notes) {
        if (value.first >= 128)
          continue;

        const auto velocity = static_cast<std::uint8_t>(muted ? 0 : value.second);
        events.push_back(
            {step.startTick, static_cast<std::uint8_t>(0x90 + channel),
             value.first, velocity});
        events.push_back(
            {std::max(step.startTick + 1, step.endTick),
             static_cast<std::uint8_t>(0x80 + channel), value.first, 0});
      } else {
        const auto controllerValue = static_cast<std::uint8_t>(
            muted || value.first >= 128 ? 0 : value.first);
        events.push_back({step.startTick,
                          static_cast<std::uint8_t>(0xB0 + channel),
                          controller, controllerValue});
      }
    }
  }

  std::stable_sort(events.begin(), events.end(), [](const Event& left,
                                                     const Event& right) {
    if (left.tick != right.tick)
      return left.tick < right.tick;
    return isNoteOff(left) && !isNoteOff(right);
  });
  return events;
}

bool writeStandardMidiFile(const std::string& path, double bpm,
                           std::vector<Event> events, std::string& error,
                           std::uint64_t endTick) {
  error.clear();
  if (!std::isfinite(bpm) || bpm <= 0.0) {
    error = "Tempo must be a positive number.";
    return false;
  }

  const auto microsPerQuarter =
      static_cast<std::uint64_t>(std::llround(60000000.0 / bpm));
  if (microsPerQuarter == 0 || microsPerQuarter > 0xFFFFFF) {
    error = "Tempo is outside the range supported by MIDI files.";
    return false;
  }

  for (const auto& event : events) {
    if (event.status < 0x80 || event.status > 0xEF ||
        event.data1 > 0x7F || event.data2 > 0x7F) {
      error = "The pattern contains an invalid MIDI event.";
      return false;
    }
  }

  std::stable_sort(events.begin(), events.end(), [](const Event& left,
                                                     const Event& right) {
    if (left.tick != right.tick)
      return left.tick < right.tick;
    return isNoteOff(left) && !isNoteOff(right);
  });

  std::vector<std::uint8_t> track;
  // Tempo: microseconds per quarter note.
  appendVariableLength(track, 0);
  track.insert(track.end(), {0xFF, 0x51, 0x03,
                             static_cast<std::uint8_t>((microsPerQuarter >> 16) & 0xFF),
                             static_cast<std::uint8_t>((microsPerQuarter >> 8) & 0xFF),
                             static_cast<std::uint8_t>(microsPerQuarter & 0xFF)});
  // 4/4 time signature.
  appendVariableLength(track, 0);
  track.insert(track.end(), {0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08});

  std::uint64_t previousTick = 0;
  for (const auto& event : events) {
    if (event.tick < previousTick ||
        event.tick - previousTick > kMaximumVariableLength) {
      error = "Pattern timing exceeds the MIDI file format limit.";
      return false;
    }
    appendVariableLength(track,
                         static_cast<std::uint32_t>(event.tick - previousTick));
    track.insert(track.end(), {event.status, event.data1, event.data2});
    previousTick = event.tick;
  }

  endTick = std::max(endTick, previousTick);
  if (endTick - previousTick > kMaximumVariableLength) {
    error = "Pattern timing exceeds the MIDI file format limit.";
    return false;
  }
  appendVariableLength(
      track, static_cast<std::uint32_t>(endTick - previousTick));
  track.insert(track.end(), {0xFF, 0x2F, 0x00});
  if (track.size() > std::numeric_limits<std::uint32_t>::max()) {
    error = "The exported pattern is too large for a MIDI file.";
    return false;
  }

  std::vector<std::uint8_t> file;
  file.insert(file.end(), {'M', 'T', 'h', 'd'});
  appendBigEndian32(file, 6);
  appendBigEndian16(file, 0);  // Single-track format.
  appendBigEndian16(file, 1);
  appendBigEndian16(file, kTicksPerQuarterNote);
  file.insert(file.end(), {'M', 'T', 'r', 'k'});
  appendBigEndian32(file, static_cast<std::uint32_t>(track.size()));
  file.insert(file.end(), track.begin(), track.end());

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    error = "Could not open the output file.";
    return false;
  }
  output.write(reinterpret_cast<const char*>(file.data()),
               static_cast<std::streamsize>(file.size()));
  if (!output) {
    error = "Could not write the complete MIDI file.";
    return false;
  }
  return true;
}

}  // namespace line::midi
