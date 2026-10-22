#include "../midi_file.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void testNoteAndChordEvents() {
  using line::midi::Mode;
  using line::midi::PatternStep;

  const std::vector<PatternStep> steps{
      {0, 480, {{60, 100}, {64, 80}}},
      {480, 960, {{60, 90}}},
      {960, 1440, {{128, 0}}},
  };
  const auto events =
      line::midi::makeEvents(steps, Mode::notes, 0, 0, false);
  require(events.size() == 6, "chord and repeated note produce on/off events");
  require(events[0].tick == 0 && events[0].status == 0x90 &&
              events[0].data1 == 60 && events[0].data2 == 100,
          "first chord note begins at tick zero");
  require(events[1].tick == 0 && events[1].data1 == 64,
          "second chord note begins at tick zero");
  require(events[2].tick == 480 && events[2].status == 0x80,
          "previous note ends before the repeated note begins");
  require(events[3].tick == 480 && events[3].status == 0x80,
          "the rest of the chord also ends on the step boundary");
  require(events[4].tick == 480 && events[4].status == 0x90,
          "repeated note begins after notes from the previous step end");
  require(events[5].tick == 960 && events[5].status == 0x80,
          "last note ends at its step boundary");
}

void testPatternTiming() {
  const line::midi::Pattern pattern{
      {{{60, 100}}, {{62, 90}}},
      {{{64, 80}}},
  };
  const auto steps = line::midi::makeSteps(pattern, 4.0);
  require(steps.size() == 3, "nested phrase becomes three timed steps");
  require(steps[0].startTick == 0 && steps[0].endTick == 479,
          "first subdivision preserves the source phrase timing");
  require(steps[1].startTick == 480 && steps[1].endTick == 959,
          "second subdivision starts after the first");
  require(steps[2].startTick == 960 && steps[2].endTick == 1919,
          "uneven nested phrase timing preserves total phrase length");
}

void testControllerEvents() {
  const std::vector<line::midi::PatternStep> steps{
      {0, 480, {{42, 0}}},
      {480, 960, {{128, 0}}},
  };
  const auto events = line::midi::makeEvents(
      steps, line::midi::Mode::controlChange, 2, 74, false);
  require(events.size() == 2, "controller pattern emits one event per value");
  require(events[0].status == 0xB2 && events[0].data1 == 74 &&
              events[0].data2 == 42,
          "controller event includes channel, controller, and value");
  require(events[1].data2 == 0, "controller rests are exported as zero");
}

void testStandardMidiFile() {
  const auto path =
      std::filesystem::temp_directory_path() / "line_midi_export_test.mid";
  const std::vector<line::midi::PatternStep> steps{
      {0, 480, {{60, 100}}},
  };
  std::string error;
  const auto events =
      line::midi::makeEvents(steps, line::midi::Mode::notes, 0, 0, false);
  require(line::midi::writeStandardMidiFile(path.string(), 120.0, events, error),
          "valid pattern writes a MIDI file");
  require(error.empty(), "successful export leaves no error");

  std::ifstream input(path, std::ios::binary);
  const std::vector<unsigned char> bytes{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  require(bytes.size() > 30, "MIDI file contains a track");
  require(bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'h' &&
              bytes[3] == 'd',
          "file has an SMF header");
  require(bytes[8] == 0 && bytes[9] == 0 && bytes[10] == 0 &&
              bytes[11] == 1,
          "file uses single-track MIDI format");
  require(bytes[12] == 0x01 && bytes[13] == 0xE0,
          "file uses 480 ticks per quarter note");
  require(bytes[22] == 0 && bytes[23] == 0xFF && bytes[24] == 0x51 &&
              bytes[25] == 0x03 && bytes[26] == 0x07 && bytes[27] == 0xA1 &&
              bytes[28] == 0x20,
          "120 BPM tempo is encoded in the track");
  require(bytes[bytes.size() - 3] == 0xFF &&
              bytes[bytes.size() - 2] == 0x2F &&
              bytes[bytes.size() - 1] == 0,
          "track ends with the required end-of-track event");
}

void testInvalidTempoIsRejected() {
  std::string error;
  require(!line::midi::writeStandardMidiFile(
              "invalid-tempo.mid", 0.0, {}, error),
          "zero tempo is rejected");
  require(!error.empty(), "invalid tempo reports an error");
}

void testTrailingRestsKeepPhraseDuration() {
  const auto path =
      std::filesystem::temp_directory_path() / "line_midi_export_rest_test.mid";
  std::string error;
  require(line::midi::writeStandardMidiFile(path.string(), 120.0, {}, error,
                                             1920),
          "pattern with trailing rests writes a MIDI file");

  std::ifstream input(path, std::ios::binary);
  const std::vector<unsigned char> bytes{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  std::error_code ignored;
  std::filesystem::remove(path, ignored);

  require(bytes.size() > 38, "trailing-rest duration is present in the track");
  require(bytes[37] == 0x8F && bytes[38] == 0,
          "end-of-track preserves the remaining phrase duration");
  require(bytes[bytes.size() - 3] == 0xFF &&
              bytes[bytes.size() - 2] == 0x2F,
          "the duration is followed by the end-of-track event");
}

}  // namespace

int main() {
  testNoteAndChordEvents();
  testPatternTiming();
  testControllerEvents();
  testStandardMidiFile();
  testInvalidTempoIsRejected();
  testTrailingRestsKeepPhraseDuration();
  std::cout << "Line MIDI file tests passed.\n";
}
