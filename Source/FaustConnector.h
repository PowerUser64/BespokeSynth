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
#include "IDrawableModule.h"
#include "Slider.h"

#include "dsp.h"
#include "UI.h"
#include "meta.h"

// NOTE: this include controls what faust module gets compiled:
#include "faust/sine-advanced.hpp"

class FaustConnector : public IAudioProcessor, public IDrawableModule
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
   // TODO(Blake):
   // void CreateUIControls() override;

   // Process
   void Process(double time) override;
   void SetEnabled(bool enabled) override;
   bool IsEnabled() const override;

private:
   void DrawModule() override;

   mydsp mDsp;

   void SetMetadataFromDSP();

   // IDEA(Blake): vector<FloatSlider>? (or similar)
};