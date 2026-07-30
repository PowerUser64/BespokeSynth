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

#pragma once

#include <mutex>
#include <vector>
#include "Checkbox.h"
#include "FaustDSP.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "TextEntry.h"
#include "faust/gui/UI.h"

class FaustUI : public UI
{
public:
   // TODO: can this be just a single parameter?
   FaustUI(int uiOriginX, int uiOriginY, IFloatSliderListener* parentFloatSlider, IDrawableModule* parentDrawableModule, ITextEntryListener* parentTextEntryListener);
   ~FaustUI();

   void UpdateUserInterface(FaustDSP& dsp);
   void UiConstructionBegin();
   void UiConstructionComplete();

   void Impl_DrawControls();
   void Impl_CheckboxUpdate(Checkbox* checkbox, double time);

   void UpdateCursorPos(int x, int y);
   void ResetCursorAndModuleBounds();
   int GetUiBottomEdgeOffset();
   int GetUiLeftEdgeOffset();

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
   enum FaustControlType
   {
      BUTTON,
      CHECKBOX,
      HSLIDER,
      VSLIDER,
      NUMENTRY
   };
   bool FreshenUiControl(const char* name, FaustControlType type);

   template <typename T>
   struct UiMeta
   {
      UiMeta(int mUiGeneration, T val)
      : generation(mUiGeneration)
      , ptr(val)
      { }
      int generation = -1;
      T ptr = 0;
   };

   template <typename T>
   UiMeta<T>* TryGetExistingControl(const char* label, std::vector<UiMeta<T>>& controls)
   {
      for (auto& control : controls)
         if (!strncmp(control.ptr->Name(), label, 100))
         {
            control.generation = mUiGeneration;
            control.ptr->SetPosition(mCursorX, mCursorY);
            return &control;
         }
   }

   int mUiGeneration = -1;
   std::mutex mUiListLock;
   std::vector<UiMeta<TextEntry*>> mTextEntries;
   std::vector<UiMeta<Checkbox*>> mCheckboxes;
   std::vector<UiMeta<FloatSlider*>> mSliders;
   std::vector<UiMeta<bool*>> mCheckboxBools;
   std::vector<UiMeta<float*>> mCheckboxFloats;
   std::vector<UiMeta<float*>> mButtonFloats;

   int mRowWidth = 100;
   int mRowHeight = 15;

   const int mSliderSizeX = 100;
   const int mSliderSizeY = 15;
   const int mCheckboxSizeX = 100;
   const int mCheckboxSizeY = 15;
   const int mTextEntrySizeX = 100;
   const int mTextEntrySizeY = 15;

   const int mElementPaddingX = 5;
   const int mElementPaddingY = 2;

   // the top-left of the UI
   const int mUiOriginX = 0;
   const int mUiOriginY = 0;

   // the "cursor" is where we'll place the next element
   int mCursorX = 0;
   int mCursorY = 0;

   int mModuleSizeX = 100;
   int mModuleSizeY = 5;

   IDrawableModule* mParentDrawableModule;
   IFloatSliderListener* mParentFloatSliderListener;
   ITextEntryListener* mParentTextEntryListener;
};