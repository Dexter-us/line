#include "line_controller.hpp"

#include "line_cids.hpp"
#include "pluginterfaces/base/ustring.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/controls/ctextlabel.h"

#include <cstring>

namespace Steinberg::Vst {
namespace {

constexpr int32 kPatternControlTag = 4101;
constexpr int32 kRunControlTag = 4102;
constexpr int32 kStatusControlTag = 4103;
constexpr std::size_t kMaxSavedPattern = 255;
constexpr char kEditorName[] = "Editor";
constexpr char kEditorResource[] = "line.uidesc";

}  // namespace

tresult PLUGIN_API LineController::initialize(FUnknown* context) {
  const auto result = EditControllerEx1::initialize(context);
  if (result != kResultOk)
    return result;
  return kResultOk;
}

tresult PLUGIN_API LineController::connect(IConnectionPoint* other) {
  const auto result = EditControllerEx1::connect(other);
  if (result == kResultOk || result == kResultTrue) {
    sendTextMessage(patternText_.c_str());
    sendTextMessage(armed_ ? "PLAY" : "STOP");
  }
  return result;
}

IPlugView* PLUGIN_API LineController::createView(FIDString name) {
  if (name && std::strcmp(name, ViewType::kEditor) == 0)
    return new VSTGUI::VST3Editor(this, kEditorName, kEditorResource);
  return nullptr;
}

VSTGUI::CView* LineController::createCustomView(
    VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes&,
    const VSTGUI::IUIDescription*, VSTGUI::VST3Editor*) {
  if (!name)
    return nullptr;
  if (std::strcmp(name, "LinePatternEditor") == 0) {
    auto* editor = new VSTGUI::CTextEdit(
        VSTGUI::CRect(), this, kPatternControlTag, patternText_.c_str());
    editor->setImmediateTextChange(true);
    patternEditor_ = editor;
    return editor;
  }
  if (std::strcmp(name, "LineRunButton") == 0) {
    auto* button = new VSTGUI::CTextButton(
        VSTGUI::CRect(), this, kRunControlTag, "PLAY / STOP",
        VSTGUI::CTextButton::kOnOffStyle);
    button->setValue(armed_ ? 1.0f : 0.0f);
    return button;
  }
  if (std::strcmp(name, "LineStatus") == 0) {
    auto* label = new VSTGUI::CTextLabel(VSTGUI::CRect(), "Pattern ready");
    statusLabel_ = label;
    return label;
  }
  return nullptr;
}

void LineController::valueChanged(VSTGUI::CControl* control) {
  if (!control)
    return;

  if (control->getTag() == kPatternControlTag) {
    auto* editor = dynamic_cast<VSTGUI::CTextEdit*>(control);
    if (!editor)
      return;
    setPattern(editor->getText().getString());
  } else if (control->getTag() == kRunControlTag) {
    armed_ = control->getValue() >= 0.5f;
    sendTextMessage(armed_ ? "PLAY" : "STOP");
    if (statusLabel_)
      statusLabel_->setText(armed_ ? "Pattern enabled" : "Pattern stopped");
  }
}

bool LineController::setPattern(std::string pattern) {
  if (pattern.size() > kMaxSavedPattern) {
    if (statusLabel_)
      statusLabel_->setText("Pattern is limited to 255 characters.");
    return false;
  }
  patternText_ = std::move(pattern);
  sendTextMessage(patternText_.c_str());
  if (statusLabel_)
    statusLabel_->setText("Pattern sent to Line.");
  return true;
}

tresult PLUGIN_API LineController::receiveText(const char8* text) {
  if (!text)
    return kInvalidArgument;
  if (statusLabel_)
    statusLabel_->setText(text);
  return kResultOk;
}

void LineController::willClose(VSTGUI::VST3Editor*) {
  patternEditor_ = nullptr;
  statusLabel_ = nullptr;
}

bool LineController::writeText(IBStream* state, const std::string& text) {
  if (!state || text.size() > kMaxSavedPattern)
    return false;
  const int32 length = static_cast<int32>(text.size());
  int32 written = 0;
  if (state->write(&length, sizeof(length), &written) != kResultOk ||
      written != sizeof(length))
    return false;
  if (length == 0)
    return true;
  written = 0;
  return state->write(text.data(), length, &written) == kResultOk &&
         written == length;
}

bool LineController::readText(IBStream* state, std::string& text) {
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

tresult PLUGIN_API LineController::setState(IBStream* state) {
  std::string restored;
  if (!readText(state, restored))
    return kResultFalse;
  patternText_ = std::move(restored);
  if (patternEditor_)
    patternEditor_->setText(patternText_.c_str());
  sendTextMessage(patternText_.c_str());
  return kResultOk;
}

tresult PLUGIN_API LineController::getState(IBStream* state) {
  return writeText(state, patternText_) ? kResultOk : kResultFalse;
}

tresult PLUGIN_API LineController::setComponentState(IBStream* state) {
  return setState(state);
}

}  // namespace Steinberg::Vst
