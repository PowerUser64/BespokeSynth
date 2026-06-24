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
#include "Checkbox.h"
#include "ClickButton.h"
#include "IDrawableModule.h"
#include "IUIControl.h"
#include "Slider.h"

/** TODO:
 * - slider quantization (int)
 * - slider curve approximation
 * - unit names?
 */

// parent should be the "this" pointer for the module that will update the sliders
FaustUI::FaustUI(IFloatSliderListener* parentFloatSliderListener, IDrawableModule* parentDrawableModule)
: mParentFloatSliderListner(parentFloatSliderListener)
, mParentDrawableModule(parentDrawableModule)
{
}

void FaustUI::DrawControls()
{
   for (auto& control : mControls)
      control->Draw();
}

void FaustUI::UpdateCursorPos(int x, int y)
{
   x += mElementPaddingX;
   y += mElementPaddingY;

   // TODO: support horizontal layouts here
   mCursorY += y;
   mModuleSizeY += y;
   mModuleSizeX = MAX(mModuleSizeX, x + mElementPaddingX);

   mParentDrawableModule->Resize(mModuleSizeX, mModuleSizeY);
}

// -- active widgets

void FaustUI::addButton(const char* label, float* zone)
{
   // TODO(Blake): Requires event handling
}

void FaustUI::addCheckButton(const char* label, float* zone)
{
   // TODO(Blake): Requires storing bools that correspond to each zone and updating the floats each frame
   // QUESTION: can we implement IDrawableModule::CheckboxUpdated() for the parent?
}
void FaustUI::addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   // redirect to hslider
   addHorizontalSlider(label, zone, init, min, max, step);
}
void FaustUI::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   mControls.push_back(new FloatSlider(mParentFloatSliderListner, label, mCursorX, mCursorY, mSliderWidth, mSliderHeight, zone, min, max));
   UpdateCursorPos(mSliderWidth, mSliderHeight);
}
void FaustUI::addNumEntry(const char* label, float* zone, float init, float min, float max, float step)
{
   // TODO(Blake): Requires storing TextEntry fields
}

// -- passive widgets

void FaustUI::addHorizontalBargraph(const char* label, float* zone, float min, float max) { }
void FaustUI::addVerticalBargraph(const char* label, float* zone, float min, float max) { }