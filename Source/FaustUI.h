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
// FaustUI.h
//

#include <vector>
#include "Slider.h"
#include "faust/gui/UI.h"

// TODO:
// - pass owner in initializer
// - NOT an IFloatSliderListener
// - FaustConnector is an IFloatSliderListener

class FaustUI : public UI
{
public:
   FaustUI(IFloatSliderListener*);

   // if a module wanted to control the drawing of its controls,
   // we would make a method for fetching the list of controls
   void DrawControls();

   // -- widget layouts

   void openTabBox(const char* label) override { }
   void openHorizontalBox(const char* label) override { }
   void openVerticalBox(const char* label) override { }
   void closeBox() override { }

   // -- active widgets

   void addButton(const char* label, float* zone) override;
   void addCheckButton(const char* label, float* zone) override;
   void addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step) override;
   void addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step) override;
   void addNumEntry(const char* label, float* zone, float init, float min, float max, float step) override;

   // -- passive widgets

   void addHorizontalBargraph(const char* label, float* zone, float min, float max) override;
   void addVerticalBargraph(const char* label, float* zone, float min, float max) override;

   // -- soundfiles

   void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override { }

private:
   std::vector<FloatSlider*> mSliders;

   int mCursorX = 5;
   int mCursorY = 5;

   const int mSliderHeight = 15;
   const int mSliderWidth = 100;

   IFloatSliderListener* mFloatsliderParent;
};