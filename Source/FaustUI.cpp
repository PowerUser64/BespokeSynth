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
// FaustUI.cpp
//

#include "FaustUI.h"
#include "Slider.h"

// parent should be the "this" pointer for the module that will update the sliders
FaustUI::FaustUI(IFloatSliderListener* parent)
{
   mFloatsliderParent = parent;
}

void FaustUI::DrawControls()
{
   for (auto& control : mSliders)
      control->Draw();
}

// -- active widgets

void FaustUI::addButton(const char* label, float* zone) { }
void FaustUI::addCheckButton(const char* label, float* zone) { }
void FaustUI::addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step) { addHorizontalSlider(label, zone, init, min, max, step); }
void FaustUI::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   FloatSlider* s = new FloatSlider(mFloatsliderParent, label, mCursorX, mCursorY, 100, 15, zone, min, max);
   mSliders.push_back(s);
   mCursorX += mSliderWidth;
   mCursorY += mSliderHeight;
}
void FaustUI::addNumEntry(const char* label, float* zone, float init, float min, float max, float step) { }

// -- passive widgets

void FaustUI::addHorizontalBargraph(const char* label, float* zone, float min, float max) { }
void FaustUI::addVerticalBargraph(const char* label, float* zone, float min, float max) { }