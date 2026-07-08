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
#include "Checkbox.h"
#include "ClickButton.h"
#include "IDrawableModule.h"
#include "Slider.h"
#include "TextEntry.h"
#include "faust/gui/UI.h"

// TODO:
// - pass owner in initializer
// - NOT an IFloatSliderListener
// - FaustConnector is an IFloatSliderListener

class FaustUI : public UI
{
public:
   // TODO: can this be just a single parameter?
   FaustUI(IFloatSliderListener* parentFloatSlider, IDrawableModule* parentDrawableModule, ITextEntryListener* parentTextEntryListener);
   ~FaustUI();

   void Impl_DrawControls();
   void Impl_CheckboxUpdate(Checkbox* checkbox, double time);

   void UpdateCursorPos(int x, int y);
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
   std::vector<IUIControl*> mControls;
   std::vector<TextEntry*> mTextEntries;
   std::vector<Checkbox*> mCheckboxes;
   std::vector<bool*> mCheckboxBools;
   std::vector<float*> mCheckboxFloats;
   std::vector<float*> mButtonFloats;

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
   IFloatSliderListener* mParentFloatSliderListner;
   ITextEntryListener* mParentTextEntryListener;
};