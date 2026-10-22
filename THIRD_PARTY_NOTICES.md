# Third-party notices

The Line source project retains its MIT license in `LICENSE`.

## Steinberg VST 3 SDK

The VST3 instrument is built against the Steinberg VST 3 SDK, licensed under the
MIT license. The Windows build pins the SDK to `v3.7.9_build_61` and fetches it
from https://github.com/steinbergmedia/vst3sdk.

## VSTGUI

VSTGUI is part of the Steinberg SDK checkout and is licensed under the 3-clause
BSD license. That permissive license is compatible with use in this MIT-licensed
project. Its full license text is provided by the SDK at `vstgui4/LICENSE`.

## pluginval

The Windows CI workflow downloads pluginval from its upstream release only to run
automated validation. pluginval is not included in or distributed with Line.
