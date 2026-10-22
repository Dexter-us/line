#include "line_pattern.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace line {
namespace {

struct ParsedNote {
  Note note{};
  bool rest = false;
};

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool parseNumber(std::string_view text, double& value) {
  if (text.empty())
    return false;
  std::string copy(text);
  char* end = nullptr;
  value = std::strtod(copy.c_str(), &end);
  return end == copy.c_str() + copy.size() && std::isfinite(value);
}

bool pitchFromToken(std::string token, std::uint8_t& pitch) {
  token = lower(std::move(token));
  double numericPitch = 0.0;
  if (parseNumber(token, numericPitch)) {
    if (numericPitch < 0.0 || numericPitch > 127.0 ||
        std::floor(numericPitch) != numericPitch)
      return false;
    pitch = static_cast<std::uint8_t>(numericPitch);
    return true;
  }

  if (token.size() < 2 || token.size() > 4)
    return false;

  const char letter = token[0];
  int semitone = -1;
  switch (letter) {
    case 'c': semitone = 0; break;
    case 'd': semitone = 2; break;
    case 'e': semitone = 4; break;
    case 'f': semitone = 5; break;
    case 'g': semitone = 7; break;
    case 'a': semitone = 9; break;
    case 'b': semitone = 11; break;
    default: return false;
  }

  std::size_t index = 1;
  if (index < token.size() &&
      (token[index] == 's' || token[index] == 'f' ||
       token[index] == '#' || token[index] == 'b')) {
    semitone += (token[index] == 's' || token[index] == '#') ? 1 : -1;
    ++index;
  }
  if (index >= token.size())
    return false;

  int octave = -1;
  for (; index < token.size(); ++index) {
    if (!std::isdigit(static_cast<unsigned char>(token[index])))
      return false;
    octave = octave < 0 ? 0 : octave;
    octave = octave * 10 + token[index] - '0';
  }
  if (octave < 0)
    return false;

  const int value = octave * 12 + semitone;
  if (value < 0 || value > 127)
    return false;
  pitch = static_cast<std::uint8_t>(value);
  return true;
}

bool parseAmplitude(std::string_view token, std::string& noteToken,
                    std::uint8_t& velocity) {
  const auto separator = token.find('~');
  if (separator == std::string_view::npos) {
    noteToken.assign(token);
    velocity = 100;
    return true;
  }
  if (token.find('~', separator + 1) != std::string_view::npos)
    return false;

  noteToken.assign(token.substr(0, separator));
  auto amplitudeText = token.substr(separator + 1);
  double amplitude = 0.0;
  if (!parseNumber(amplitudeText, amplitude) || amplitude < 0.0 ||
      amplitude > 1.0)
    return false;
  velocity = static_cast<std::uint8_t>(std::lround(amplitude * 127.0));
  return true;
}

bool parseSingleNote(std::string_view token, ParsedNote& parsed) {
  if (token == "-") {
    parsed.rest = true;
    return true;
  }

  std::string noteToken;
  std::uint8_t velocity = 100;
  if (!parseAmplitude(token, noteToken, velocity) || noteToken.empty())
    return false;

  std::uint8_t pitch = 0;
  if (!pitchFromToken(noteToken, pitch))
    return false;

  parsed.note = {pitch, velocity};
  return true;
}

void skipSpaces(std::string_view source, std::size_t& cursor) {
  while (cursor < source.size() &&
         std::isspace(static_cast<unsigned char>(source[cursor])))
    ++cursor;
}

bool parseStep(std::string_view source, std::size_t& cursor, Step& step,
               std::string& error) {
  skipSpaces(source, cursor);
  if (cursor >= source.size())
    return false;

  if (source[cursor] == '.') {
    step.beats = 0.5;
    ++cursor;
  }

  if (cursor < source.size() && source[cursor] == '(') {
    ++cursor;
    bool foundNote = false;
    while (cursor < source.size()) {
      skipSpaces(source, cursor);
      if (cursor < source.size() && source[cursor] == ')') {
        ++cursor;
        break;
      }
      const auto start = cursor;
      while (cursor < source.size() &&
             !std::isspace(static_cast<unsigned char>(source[cursor])) &&
             source[cursor] != ')')
        ++cursor;
      if (start == cursor) {
        error = "Expected a note inside the chord.";
        return false;
      }
      ParsedNote parsed;
      if (!parseSingleNote(source.substr(start, cursor - start), parsed)) {
        error = "Invalid note in chord.";
        return false;
      }
      if (!parsed.rest) {
        if (step.noteCount == kMaxChordNotes) {
          error = "A chord can contain at most 16 notes.";
          return false;
        }
        step.notes[step.noteCount++] = parsed.note;
        foundNote = true;
      }
    }
    if (cursor == 0 || source[cursor - 1] != ')') {
      error = "Chord is missing a closing parenthesis.";
      return false;
    }
    if (!foundNote) {
      error = "A chord must contain at least one note.";
      return false;
    }

    if (cursor < source.size() && source[cursor] == '~') {
      const auto ampStart = cursor++;
      while (cursor < source.size() &&
             (std::isdigit(static_cast<unsigned char>(source[cursor])) ||
              source[cursor] == '.'))
        ++cursor;
      std::string amplitudeToken = "c0";
      amplitudeToken.append(source.substr(ampStart, cursor - ampStart));
      ParsedNote amplitudeNote;
      if (!parseSingleNote(amplitudeToken, amplitudeNote)) {
        error = "Chord velocity must be between 0 and 1.";
        return false;
      }
      for (std::size_t i = 0; i < step.noteCount; ++i)
        step.notes[i].velocity = amplitudeNote.note.velocity;
    }
  } else {
    const auto start = cursor;
    while (cursor < source.size() &&
           !std::isspace(static_cast<unsigned char>(source[cursor])) &&
           source[cursor] != ')')
      ++cursor;
    if (start == cursor) {
      error = "Expected a MIDI note, note name, rest, or chord.";
      return false;
    }
    auto token = source.substr(start, cursor - start);
    if (!token.empty() && token.back() == '.') {
      step.beats = 1.5;
      token.remove_suffix(1);
    }
    ParsedNote parsed;
    if (!parseSingleNote(token, parsed)) {
      error = "Invalid note or velocity.";
      return false;
    }
    if (!parsed.rest) {
      step.notes[0] = parsed.note;
      step.noteCount = 1;
    }
  }

  if (cursor < source.size() && source[cursor] == '.') {
    step.beats = 1.5;
    ++cursor;
  }
  if (step.beats <= 0.0)
    step.beats = 1.0;
  return true;
}

}  // namespace

double Pattern::lengthInBeats() const noexcept {
  double length = 0.0;
  for (std::size_t i = 0; i < stepCount; ++i)
    length += steps[i].beats;
  return length;
}

ParseResult parsePattern(std::string_view source, double defaultBpm) {
  ParseResult result;
  if (!std::isfinite(defaultBpm) || defaultBpm < 20.0 || defaultBpm > 300.0) {
    result.error = "Tempo must be between 20 and 300 BPM.";
    return result;
  }
  result.pattern.bpm = defaultBpm;

  std::size_t cursor = 0;
  while (cursor < source.size()) {
    skipSpaces(source, cursor);
    if (cursor >= source.size())
      break;

    if (source.substr(cursor, 3) == "bpm" ||
        source.substr(cursor, 3) == "BPM") {
      cursor += 3;
      const auto start = cursor;
      while (cursor < source.size() &&
             (std::isdigit(static_cast<unsigned char>(source[cursor])) ||
              source[cursor] == '.'))
        ++cursor;
      double tempo = 0.0;
      if (!parseNumber(source.substr(start, cursor - start), tempo) ||
          tempo < 20.0 || tempo > 300.0) {
        result.error = "Tempo command must be between 20 and 300 BPM.";
        return result;
      }
      result.pattern.bpm = tempo;
      continue;
    }

    if (result.pattern.stepCount == kMaxSteps) {
      result.error = "A pattern can contain at most 128 steps.";
      return result;
    }
    Step step;
    if (!parseStep(source, cursor, step, result.error))
      return result;
    result.pattern.steps[result.pattern.stepCount++] = step;
  }

  if (result.pattern.stepCount == 0)
    result.error = "Enter at least one note, rest, or chord.";
  return result;
}

}  // namespace line
