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

#include "IAudioProcessor.h"
#include "TextEntry.h"
#include "IDrawableModule.h"
#include "FaustUI.h"
#include "Slider.h"
#include <array>
#include "faust/dsp/interpreter-dsp.h"

// Faust includes (not directly used, TODO: figure out how to include these in the compiled program)
#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"
#include "faust/gui/meta.h"

// TODO(Blake):
// This define exists because we need an array of pointers to channels for
// faust to process, and for memory management reasons, it's nice for its size
// to be known at compile time.
// TODO: check that the number of channels in the faust program is less than FAUST_MAX_CHANNELS (at compile time, in cmake)
#define FAUST_MAX_CHANNELS 2

class FaustConnector : public IAudioProcessor, public IDrawableModule, public ITextEntryListener, public IFloatSliderListener
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
   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override { };
   void DrawModule() override;
   void CheckboxUpdated(Checkbox* checkbox, double time) override;
   void TextEntryComplete(TextEntry* entry) override;

   // Process
   void Process(double time) override;
   void SetEnabled(bool enabled) override;
   bool IsEnabled() const override;

private:
   std::array<float*, FAUST_MAX_CHANNELS> mInChannels = { 0 };
   std::array<float*, FAUST_MAX_CHANNELS> mOutChannels = { 0 };

   interpreter_dsp* mDsp;
   FaustUI mDspUi;

   interpreter_dsp_factory* mDspFactory;
   // TODO: watch a file instead
   std::string mDspString;

   std::string mFaustLibPath;
   std::array<const char*, 2> mFaustFactoryArgv;

   void SetMetadataFromDSP();
};