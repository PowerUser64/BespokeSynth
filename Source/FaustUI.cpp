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
#include "IDrawableModule.h"
#include "Slider.h"
#include "SynthGlobals.h"
#include "TextEntry.h"
#include <cstring>
#include <sys/poll.h>

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
   // Move the cursor to its starting position
   mCursorX = mUiOriginX + mElementPaddingX;
   mCursorY = mUiOriginY + mElementPaddingY;
}

FaustUI::~FaustUI()
{
}

void FaustUI::Impl_DrawControls()
{
   for (auto& control : mTextEntries)
      control.ptr->Draw();
   for (auto& control : mSliders)
      control.ptr->Draw();
   for (auto& control : mCheckboxes)
      control.ptr->Draw();
}

void FaustUI::Impl_CheckboxUpdate(Checkbox* checkbox, double time)
{
   for (int i = 0; i < mCheckboxes.size(); ++i)
   {
      if (checkbox == mCheckboxes[i].ptr)
      {
         if (*mCheckboxBools[i].ptr)
            *mCheckboxFloats[i].ptr = 1.0f;
         else
            *mCheckboxFloats[i].ptr = 0.0f;
      }
   }
}

int FaustUI::GetUiBottomEdgeOffset()
{
   return mCursorY;
}

int FaustUI::GetUiLeftEdgeOffset()
{
   return mElementPaddingX;
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

// take note of any controls we have, so we can diff it against the new list
void FaustUI::UiConstructionBegin()
{
   mUiGeneration += 1;
}

#define FILTER_CURRENT_GEN_UI_LIST(list)       \
   for (auto& control : list)                  \
   {                                           \
      if (control.generation != mUiGeneration) \
      {                                        \
         control.ptr->Delete();                \
         control.ptr = 0;                      \
      }                                        \
   }

// TODO: swap with the end of the list
#define FILTER_CURRENT_GEN_UI_PARAM_LIST(list) \
   for (auto& control : list)                  \
   {                                           \
      if (control.generation != mUiGeneration) \
      {                                        \
         delete control.ptr;                   \
         control.ptr = 0;                      \
      }                                        \
   }

// find any controls that don't exist any more and clean them up
void FaustUI::UiConstructionComplete(){
   // delete old controls
   FILTER_CURRENT_GEN_UI_LIST(mTextEntries)
   FILTER_CURRENT_GEN_UI_LIST(mCheckboxes)
   FILTER_CURRENT_GEN_UI_LIST(mSliders)
   FILTER_CURRENT_GEN_UI_PARAM_LIST(mCheckboxBools)
   FILTER_CURRENT_GEN_UI_PARAM_LIST(mCheckboxFloats)
   FILTER_CURRENT_GEN_UI_PARAM_LIST(mButtonFloats)
}

#undef FILTER_CURRENT_GEN_UI_LIST
#undef FILTER_CURRENT_GEN_UI_PARAM_LIST

#define TRY_EXISTING_CONTROL(found, label, controls)   \
   for (auto& control : controls)                      \
      if (strncmp(control.ptr->Name(), label, 100))    \
      {                                                \
         found = true;                                 \
         control.generation = mUiGeneration;           \
         control.ptr->SetPosition(mCursorX, mCursorY); \
         break;                                        \
      }

// -- active widgets

void FaustUI::addButton(const char* label, float* zone)
{
   // faust buttons map nicely to bespoke checkboxes
   addCheckButton(label, zone);
}
void FaustUI::addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   // redirect to hslider
   addHorizontalSlider(label, zone, init, min, max, step);
}

void FaustUI::addCheckButton(const char* label, float* zone)
{
   bool foundOld = false;
   TRY_EXISTING_CONTROL(foundOld, label, mCheckboxes)

   if (!foundOld)
   {
      UiMeta<bool*> newBool(mUiGeneration, new bool(false));
      UiMeta<Checkbox*> checkbox(mUiGeneration, new Checkbox(mParentDrawableModule, label, mCursorX, mCursorY, newBool.ptr));

      mCheckboxFloats.push_back(UiMeta<float*>(mUiGeneration, zone));
      mCheckboxBools.push_back(newBool);

      mCheckboxes.push_back(checkbox);
   }

   UpdateCursorPos(mCheckboxSizeX, mCheckboxSizeY);
}
void FaustUI::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   bool foundOld = false;
   TRY_EXISTING_CONTROL(foundOld, label, mCheckboxes)

   if (!foundOld)
   {
      FloatSlider* slider = new FloatSlider(mParentFloatSliderListner, label, mCursorX, mCursorY, mSliderSizeX, mSliderSizeY, zone, min, max);

      mSliders.push_back(UiMeta<FloatSlider*>(mUiGeneration, slider));
   }

   UpdateCursorPos(mSliderSizeX, mSliderSizeY);
}
void FaustUI::addNumEntry(const char* label, float* zone, float init, float min, float max, float step)
{
   bool foundOld = false;
   TRY_EXISTING_CONTROL(foundOld, label, mCheckboxes)

   if (!foundOld)
   {
      UiMeta<TextEntry*> textentry(mUiGeneration, new TextEntry(mParentTextEntryListener, label, mCursorX, mCursorY, 5, zone, min, max));

      mTextEntries.push_back(textentry);
   }

   UpdateCursorPos(mTextEntrySizeX, mTextEntrySizeY);
}

// -- passive widgets

void FaustUI::addHorizontalBargraph(const char* label, float* zone, float min, float max) { }
void FaustUI::addVerticalBargraph(const char* label, float* zone, float min, float max) { }

#undef TRY_EXISTING_CONTROL