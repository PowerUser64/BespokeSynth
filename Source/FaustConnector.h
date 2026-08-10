/**
    bespoke synth, a software modular synthesizer
    Copyright (C) 2021 Ryan Challinor (contact: awwbees@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

//
//  FaustConnector.h
//

#pragma once

#include "Checkbox.h"
#include "ClickButton.h"
#include "CodeEntry.h"
#include "IAudioProcessor.h"
#include "PoliteDoubleBuffer.h"
#include "TextEntry.h"
#include "IDrawableModule.h"
#include "FaustUI.h"
#include "FaustDSP.h"
#include "Slider.h"
#include "faust/dsp/interpreter-dsp.h"

// Faust includes (not directly used, TODO: figure out how to include these in the compiled program)
#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"

// TODO: check that the number of channels in the faust program is less than FAUST_MAX_CHANNELS

class FaustConnector : public IAudioProcessor, public IDrawableModule, public ITextEntryListener, public IFloatSliderListener, public ICodeEntryListener, public IButtonListener
{
public:
   // Module interface
   FaustConnector();
   virtual ~FaustConnector();
   static bool AcceptsAudio();
   static bool AcceptsNotes();
   static bool AcceptsPulses();
   static IDrawableModule* Create();

   // UI
   void CreateUIControls() override;
   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override {};
   void DrawModule() override;
   void CheckboxUpdated(Checkbox* checkbox, double time) override;
   void TextEntryComplete(TextEntry* entry) override;

   // UI: Editor
   void ExecuteCode() override;
   void LoadState(FileStreamIn& in, int rev) override;
   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   std::pair<int, int> ExecuteBlock(int lineStart, int lineEnd) override { return std::pair<int, int>(); }

   // Process
   void Process(double time) override;
   void SetEnabled(bool enabled) override;
   bool IsEnabled() const override;

private:
   FaustChannelArray mInChannels = { 0 };
   FaustChannelArray mOutChannels = { 0 };

   void UpdateDspFromEditorBox();
   void UpdateDspFromString(std::string dspString);
   void HandleFaustError();
   void Impl_Process(double time);

   // UI: edit/run/optimize
   CodeEntry* mDspEditorBox = 0;
   Checkbox* mUiEditCheckbox = 0;
   ClickButton* mUiRunButton = 0;
   ClickButton* mUiOptimizeButton = 0;

   static constexpr int mUiOriginX = 5;
   static constexpr int mUiOriginY = 2;
   static constexpr int mUiMinWidth = 120;
   static constexpr int mUiMinHeight = 19;

   static constexpr int mUiLayoutSpacing = 2;
   static constexpr int mUiLayoutWidthCheckbox = 37;
   static constexpr int mUiLayoutWidthRunButton = 22;
   static constexpr int mUiLayoutWidthOptimizeButton = 52;

   static constexpr int mUiControlsWidth = mUiLayoutWidthCheckbox + mUiLayoutSpacing + mUiLayoutWidthRunButton + mUiLayoutSpacing + mUiLayoutWidthOptimizeButton + mUiLayoutSpacing;
   static constexpr int mUiControlsHeight = 15;

   void ButtonClicked(ClickButton* button, double time) override;

   PoliteDoubleBuffer<FaustDSP> mDspDoubleBuf;
   FaustUI mDspUi;
   bool mEditMode = false;
};