#include "line_controller.hpp"
#include "line_cids.hpp"
#include "line_processor.hpp"

#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF("Dexter U.S.", "https://github.com/Dexter-us/line", "")

DEF_CLASS2(INLINE_UID_FROM_FUID(LineProcessorUID),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           "Line",
           Vst::kDistributable,
           "Instrument|Synth",
           "0.1.0",
           kVstVersionString,
           LineProcessor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(LineControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           "Line Controller",
           0,
           "",
           "0.1.0",
           kVstVersionString,
           LineController::createInstance)

END_FACTORY
