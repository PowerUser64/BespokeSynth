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
#include "TextEntry.h"

/** TODO:
 * - slider quantization (int)
 * - slider curve approximation
 * - unit names?
 */

// parent should be the "this" pointer for the module that will update the sliders
FaustUI::FaustUI(IFloatSliderListener* parentFloatSliderListener, IDrawableModule* parentDrawableModule, ITextEntryListener* parentTextEntryListener)
: mParentDrawableModule(parentDrawableModule)
, mParentFloatSliderListner(parentFloatSliderListener)
, mParentTextEntryListener(parentTextEntryListener)
{
}

void FaustUI::Impl_DrawControls()
{
   for (auto& control : mControls)
      control->Draw();
}

void FaustUI::Impl_CheckboxUpdate(Checkbox* checkbox, double time)
{
   for (int i = 0; i < mCheckboxes.size(); ++i)
   {
      if (checkbox == mCheckboxes[i])
      {
         if (*mCheckboxBools[i])
            *mCheckboxFloats[i] = 1.0f;
         else
            *mCheckboxFloats[i] = 0.0f;
      }
   }
}

void FaustUI::Impl_TextEntryComplete(TextEntry* entry)
{
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
   // buttons in bespoke are events, but faust doesn't have any notion of an
   addCheckButton(label, zone);
}

void FaustUI::addCheckButton(const char* label, float* zone)
{
   bool* checkboxBool = new bool(false);
   Checkbox* checkbox = new Checkbox(mParentDrawableModule, label, mCursorX, mCursorY, checkboxBool);

   mCheckboxFloats.push_back(zone);
   mCheckboxBools.push_back(checkboxBool);

   mCheckboxes.push_back(checkbox);
   mControls.push_back(checkbox);

   UpdateCursorPos(mCheckboxSizeX, mCheckboxSizeY);
}
void FaustUI::addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   // redirect to hslider
   addHorizontalSlider(label, zone, init, min, max, step);
}
void FaustUI::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   mControls.push_back(new FloatSlider(mParentFloatSliderListner, label, mCursorX, mCursorY, mSliderSizeX, mSliderSizeY, zone, min, max));
   UpdateCursorPos(mSliderSizeX, mSliderSizeY);
}
void FaustUI::addNumEntry(const char* label, float* zone, float init, float min, float max, float step)
{
   TextEntry* textentry = new TextEntry(mParentTextEntryListener, label, mCursorX, mCursorY, 5, zone, min, max);

   mTextEntries.push_back(textentry);
   mControls.push_back(textentry);

   UpdateCursorPos(mTextEntrySizeX, mTextEntrySizeY);
}

// -- passive widgets

void FaustUI::addHorizontalBargraph(const char* label, float* zone, float min, float max) { }
void FaustUI::addVerticalBargraph(const char* label, float* zone, float min, float max) { }