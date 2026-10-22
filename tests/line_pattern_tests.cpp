#include "../plugin/line_pattern.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void testNumericNotes() {
  const auto parsed = line::parsePattern("45 46 47 48");
  require(static_cast<bool>(parsed), "numeric notes parse");
  require(parsed.pattern.stepCount == 4, "four numeric steps");
  require(parsed.pattern.steps[0].notes[0].pitch == 45, "numeric pitch retained");
  require(parsed.pattern.steps[3].notes[0].pitch == 48, "last numeric pitch retained");
}

void testNoteNamesAndDurations() {
  const auto parsed = line::parsePattern("c3 .as2 b2. c4 cs4 eb4");
  require(static_cast<bool>(parsed), "note names and durations parse");
  require(parsed.pattern.steps[0].notes[0].pitch == 36, "C3 follows upstream octave mapping");
  require(parsed.pattern.steps[1].notes[0].pitch == 34, "A-sharp 2 follows upstream mapping");
  require(std::abs(parsed.pattern.steps[1].beats - 0.5) < 0.0001, "leading dot is an eighth note");
  require(std::abs(parsed.pattern.steps[2].beats - 1.5) < 0.0001, "trailing dot is dotted quarter");
  require(parsed.pattern.steps[5].notes[0].pitch == 51, "E-flat 4 follows upstream mapping");
}

void testChordsRestsAndVelocity() {
  const auto parsed = line::parsePattern("(c3 e3 g3)~.7 - d3~.5");
  if (!parsed)
    std::cerr << "Chord parse detail: " << parsed.error << '\n';
  require(static_cast<bool>(parsed), "chord, rest, and velocity parse");
  require(parsed.pattern.steps[0].noteCount == 3, "chord has three notes");
  require(parsed.pattern.steps[0].notes[0].velocity == 89, "chord velocity is applied");
  require(parsed.pattern.steps[1].noteCount == 0, "rest contains no notes");
  require(parsed.pattern.steps[2].notes[0].velocity == 64, "single-note velocity parses");
}

void testTempoAndErrors() {
  const auto withTempo = line::parsePattern("bpm140 c4");
  require(static_cast<bool>(withTempo), "tempo command parses");
  require(std::abs(withTempo.pattern.bpm - 140.0) < 0.001, "tempo command is retained");
  require(!line::parsePattern("h9").error.empty(), "invalid note is rejected");
  require(!line::parsePattern("(c3 e3").error.empty(), "unclosed chord is rejected");
  require(!line::parsePattern("").error.empty(), "empty pattern is rejected");
  require(!line::parsePattern("c3", 500.0).error.empty(), "invalid default tempo is rejected");
}

}  // namespace

int main() {
  testNumericNotes();
  testNoteNamesAndDurations();
  testChordsRestsAndVelocity();
  testTempoAndErrors();
  std::cout << "Line pattern tests passed.\n";
}
