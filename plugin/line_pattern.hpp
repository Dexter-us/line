#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace line {

constexpr std::size_t kMaxSteps = 128;
constexpr std::size_t kMaxChordNotes = 16;

struct Note {
  std::uint8_t pitch = 0;
  std::uint8_t velocity = 127;
};

struct Step {
  std::array<Note, kMaxChordNotes> notes{};
  std::uint8_t noteCount = 0;
  double beats = 1.0;
};

struct Pattern {
  std::array<Step, kMaxSteps> steps{};
  std::size_t stepCount = 0;
  double bpm = 120.0;

  double lengthInBeats() const noexcept;
};

struct ParseResult {
  Pattern pattern{};
  std::string error;

  explicit operator bool() const noexcept { return error.empty(); }
};

// Supports the original line note names and MIDI numbers, rests, chords,
// per-note velocity (for example c3~.7), and eighth/dotted-quarter steps.
ParseResult parsePattern(std::string_view source, double defaultBpm = 120.0);

}  // namespace line
