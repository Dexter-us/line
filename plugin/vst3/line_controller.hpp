#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include <string>

namespace Steinberg::Vst {

class LineController final : public EditControllerEx1,
                             public VSTGUI::VST3EditorDelegate,
                             public VSTGUI::IControlListener {
 public:
  static FUnknown* createInstance(void*) {
    return static_cast<IEditController*>(new LineController());
  }

  tresult PLUGIN_API initialize(FUnknown* context) override;
  tresult PLUGIN_API connect(IConnectionPoint* other) override;
  tresult PLUGIN_API setState(IBStream* state) override;
  tresult PLUGIN_API getState(IBStream* state) override;
  tresult PLUGIN_API setComponentState(IBStream* state) override;
  tresult PLUGIN_API receiveText(const char8* text) override;
  IPlugView* PLUGIN_API createView(FIDString name) override;

  VSTGUI::CView* createCustomView(
      VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
      const VSTGUI::IUIDescription* description,
      VSTGUI::VST3Editor* editor) override;
  void valueChanged(VSTGUI::CControl* control) override;
  void willClose(VSTGUI::VST3Editor* editor) override;

 private:
  bool setPattern(std::string pattern);
  static bool writeText(IBStream* state, const std::string& text);
  static bool readText(IBStream* state, std::string& text);

  std::string patternText_ = "c3 e3 g3 c4";
  VSTGUI::CTextEdit* patternEditor_ = nullptr;
  VSTGUI::CTextLabel* statusLabel_ = nullptr;
  bool armed_ = true;
};

}  // namespace Steinberg::Vst
