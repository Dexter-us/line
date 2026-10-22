#include "line_processor.hpp"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

float renderPeak(LineProcessor& processor, ProcessContext& context,
                int32 sampleCount) {
  std::vector<Sample32> left(static_cast<std::size_t>(sampleCount), 0.0f);
  std::vector<Sample32> right(static_cast<std::size_t>(sampleCount), 0.0f);
  Sample32* channels[] = {left.data(), right.data()};

  AudioBusBuffers output{};
  output.numChannels = 2;
  output.channelBuffers32 = channels;

  ProcessData data;
  data.numSamples = sampleCount;
  data.numOutputs = 1;
  data.outputs = &output;
  data.processContext = &context;
  processor.process(data);

  float peak = 0.0f;
  for (int32 sample = 0; sample < sampleCount; ++sample) {
    peak = std::max(peak, std::abs(left[static_cast<std::size_t>(sample)]));
    peak = std::max(peak, std::abs(right[static_cast<std::size_t>(sample)]));
  }
  return peak;
}

}  // namespace

int main() {
  LineProcessor processor;

  ProcessSetup setup{};
  setup.sampleRate = 44100.0;
  setup.maxSamplesPerBlock = 8192;
  setup.processMode = kRealtime;
  setup.symbolicSampleSize = kSample32;
  require(processor.setupProcessing(setup) == kResultOk,
          "processor accepts the test audio format");

  require(processor.receiveText(
              "bpm120 (c5 e5 g5)~.7 - d5~.5 .g5 b5.") == kResultOk,
          "processor accepts the pattern");
  require(processor.receiveText("PLAY") == kResultOk,
          "processor starts playback");

  ProcessContext context{};
  context.state = 0;
  require(renderPeak(processor, context, 8192) > 0.01f,
          "pattern produces audio while the DAW transport is stopped");

  require(processor.receiveText("STOP") == kResultOk,
          "processor stops playback");
  renderPeak(processor, context, 4096);
  require(renderPeak(processor, context, 512) < 0.0001f,
          "stopped pattern releases its audio voices");

  std::cout << "Line audio output tests passed.\n";
  return 0;
}
