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
#include "FaustDSP.h"
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
FaustUI::FaustUI(int uiOriginX, int uiOriginY, IFloatSliderListener* parentFloatSliderListener, IDrawableModule* parentDrawableModule, ITextEntryListener* parentTextEntryListener)
: mUiOriginX(uiOriginX)
, mUiOriginY(uiOriginY)
, mParentDrawableModule(parentDrawableModule)
, mParentFloatSliderListener(parentFloatSliderListener)
, mParentTextEntryListener(parentTextEntryListener)
{
   ResetCursorAndModuleBounds();
}

FaustUI::~FaustUI()
{
   for (auto& control : mCheckboxBools)
      delete control.ptr;
}

void FaustUI::Impl_DrawControls()
{
   mUiListLock.lock();

   for (auto& control : mTextEntries)
      if (control.generation == mUiGeneration)
         control.ptr->Draw();

   for (auto& control : mSliders)
      if (control.generation == mUiGeneration)
         control.ptr->Draw();

   for (auto& control : mCheckboxes)
      if (control.generation == mUiGeneration)
         control.ptr->Draw();

   mUiListLock.unlock();
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

int FaustUI::GetUiHeight()
{
   return mCursorY - mUiOriginY;
}

int FaustUI::GetUiWidth()
{
   return mCursorX + mRowWidth - mUiOriginX;
}

int FaustUI::GetUiLeftEdgeOffset()
{
   return mElementPaddingX + mUiOriginX;
}

void FaustUI::UpdateCursorPos(int x, int y)
{
   x += mElementPaddingX;
   y += mElementPaddingY;

   // TODO: support horizontal layouts here
   mCursorY += y;
   mModuleSizeY += y;
   mModuleSizeX = MAX(mModuleSizeX, x + mElementPaddingX);
}

void FaustUI::ResetCursorAndModuleBounds()
{
   // Move the cursor to its starting position
   mCursorX = mUiOriginX;
   mCursorY = mUiOriginY;
   mModuleSizeX = mCursorX;
   mModuleSizeY = mCursorY;
}

#define FILTER_CURRENT_GEN_UI_LIST(list)                            \
   for (auto& control : list)                                       \
   {                                                                \
      if (control.generation != mUiGeneration)                      \
      {                                                             \
         mParentDrawableModule->RemoveUIControl(control.ptr, true); \
         control.ptr = 0;                                           \
      }                                                             \
   }

// TODO: swap with the end of the list
#define FILTER_CURRENT_GEN_UI_PARAM_LIST(list) \
   for (auto& control : list)                  \
   {                                           \
      if (control.generation != mUiGeneration) \
      {                                        \
         control.ptr = 0;                      \
      }                                        \
   }

void FaustUI::UpdateUserInterface(FaustDSP& dsp)
{
   mUiListLock.lock();
   UiConstructionBegin();
   dsp.BuildUserInterface(this);
   UiConstructionComplete();
   mUiListLock.unlock();
}

// take note of any controls we have, so we can diff it against the new list
void FaustUI::UiConstructionBegin()
{
   ResetCursorAndModuleBounds();
   mUiGeneration += 1;
}

// find any controls that don't exist any more and clean them up
void FaustUI::UiConstructionComplete()
{
   // TODO: fix control deletion
   // // delete old controls
   // FILTER_CURRENT_GEN_UI_LIST(mTextEntries)
   // FILTER_CURRENT_GEN_UI_LIST(mCheckboxes)
   // FILTER_CURRENT_GEN_UI_LIST(mSliders)
   // FILTER_CURRENT_GEN_UI_PARAM_LIST(mCheckboxBools)
   // FILTER_CURRENT_GEN_UI_PARAM_LIST(mCheckboxFloats)
   // FILTER_CURRENT_GEN_UI_PARAM_LIST(mButtonFloats)
}

#undef FILTER_CURRENT_GEN_UI_LIST
#undef FILTER_CURRENT_GEN_UI_PARAM_LIST

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
   UiMeta<Checkbox*> control = GetNewOrExistingUiControl(label, mCheckboxes);

   if (control.ptr)
   {
      control.ptr->SetDefaultValue(*zone);
   }
   else
   {
      UiMeta<bool*> newBool(mUiGeneration, new bool(false));
      control.ptr = new Checkbox(mParentDrawableModule, label, mCursorX, mCursorY, newBool.ptr);
      control.ptr->SetDefaultValue(*zone);

      mCheckboxBools.push_back(newBool);

      mCheckboxes.push_back(control);
   }
   mCheckboxFloats.push_back(UiMeta<float*>(mUiGeneration, zone));

   UpdateCursorPos(mCheckboxSizeX, mCheckboxSizeY);
}
void FaustUI::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
{
   UiMeta<FloatSlider*> control = GetNewOrExistingUiControl(label, mSliders);

   if (control.ptr)
   {
      control.ptr->SetVar(zone);
      control.ptr->SetDefaultValue(*zone);
   }
   else
   {
      control.ptr = new FloatSlider(mParentFloatSliderListener, label, mCursorX, mCursorY, mSliderSizeX, mSliderSizeY, zone, min, max);
      control.ptr->SetDefaultValue(*zone);

      mSliders.push_back(control);
   }

   UpdateCursorPos(mSliderSizeX, mSliderSizeY);
}
void FaustUI::addNumEntry(const char* label, float* zone, float init, float min, float max, float step)
{
   UiMeta<TextEntry*> control = GetNewOrExistingUiControl(label, mTextEntries);

   if (control.ptr)
   {
      control.ptr->SetVar(zone);
      control.ptr->SetDefaultValue(*zone);
   }
   else
   {
      UiMeta<TextEntry*> textentry(mUiGeneration, new TextEntry(mParentTextEntryListener, label, mCursorX, mCursorY, 5, zone, min, max));
      textentry.ptr->SetDefaultValue(*zone);

      mTextEntries.push_back(textentry);
   }

   UpdateCursorPos(mTextEntrySizeX, mTextEntrySizeY);
}

// -- passive widgets

void FaustUI::addHorizontalBargraph(const char* label, float* zone, float min, float max) {}
void FaustUI::addVerticalBargraph(const char* label, float* zone, float min, float max) {}

#undef TRY_EXISTING_CONTROL