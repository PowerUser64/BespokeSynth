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
// FaustConnector.cpp
//

#include "FaustConnector.h"

#include "ChannelBuffer.h"
#include "FaustDSP.h"
#include "IAudioReceiver.h"
#include "ModularSynth.h" // save/load
#include "OpenFrameworksPort.h"
#include "Profiler.h" // profiling
#include "SynthGlobals.h"
#include <cassert>

FaustConnector::~FaustConnector() { };

FaustConnector::FaustConnector()
: IAudioProcessor(gBufferSize)
, IDrawableModule(120, 10)
, mDspDoubleBuf(FaustDSP("process = _, _;"), FaustDSP("process = _, _;"))
, mDspUi(this, this, this)
{
}

// TODO(Blake): Faust modules need to be initialized before we can say how much IO they have
bool FaustConnector::AcceptsAudio() { return true; }
bool FaustConnector::AcceptsNotes() { return false; }
bool FaustConnector::AcceptsPulses() { return false; }

IDrawableModule* FaustConnector::Create()
{
   FaustConnector* ret = new FaustConnector();
   if (ret->mDspDoubleBuf.GetCurrent().IsReady())
   {
      // TODO: when error messages are implemented, report these as errors in the UI:
      assert(ret->mDspDoubleBuf.GetCurrent().GetNumOutputs() <= FAUST_MAX_CHANNELS);
      assert(ret->mDspDoubleBuf.GetCurrent().GetNumInputs() <= FAUST_MAX_CHANNELS);
   }
   return ret;
}

void FaustConnector::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   mDspEditorBox = new CodeEntry(this, "__dsp_editor", 3, 100, 300, 300);

   UpdateDspFromEditorBox();
}

void FaustConnector::UpdateDspFromEditorBox()
{
   mDspDoubleBuf.GetOther().UpdateDsp(mDspEditorBox->GetText(true));
   if (mDspDoubleBuf.GetOther().IsReady() == true)
   {
      mEditMode = false;
      mDspDoubleBuf.GetOther().BuildUserInterface(&mDspUi);
      mDspDoubleBuf.SwitchWhenReady();
   }
}

void FaustConnector::HandleFaustError()
{
   if (mDspDoubleBuf.GetCurrent().IsReady() == false)
      return;

   ofLog() << "Faust error:" << '\n'
           << mFaustErrorStr;
}

bool FaustConnector::IsEnabled() const
{
   return mEnabled;
}

void FaustConnector::SetEnabled(bool enabled)
{
   mEnabled = enabled;
}

void FaustConnector::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mDspUi.Impl_DrawControls();

   mDspEditorBox->SetShowing(mEditMode);
   if (mEditMode)
   {
      mDspEditorBox->SetPosition(mDspUi.GetUiLeftEdgeOffset(), mDspUi.GetUiBottomEdgeOffset());

      ofRectangle rect = mDspEditorBox->GetRect(K(local));
      mWidth = MAX(mWidth, rect.x + rect.width + 3);
      mHeight = rect.y + rect.height + 3;

      mDspEditorBox->Draw();
   }
}

void FaustConnector::KeyPressed(int key, bool isRepeat)
{
   IDrawableModule::KeyPressed(key, isRepeat);

   if (gHoveredModule == this)
   {
      if (key == 'e' && GetKeyModifiers() == kModifier_Command)
      {
         if (mEditMode == false)
         {
            mDspEditorBox->SetText(mDspDoubleBuf.GetCurrent().GetDspString());
            mDspEditorBox->ResetScroll();
            IKeyboardFocusListener::SetActiveKeyboardFocus(mDspEditorBox);
            mEditMode = true;
         }
      }
   }
}

void FaustConnector::ExecuteCode()
{
   if (mEditMode == false)
      return;

   UpdateDspFromEditorBox();
}

void FaustConnector::CheckboxUpdated(Checkbox* checkbox, double time)
{
   mDspUi.Impl_CheckboxUpdate(checkbox, time);
}

void FaustConnector::TextEntryComplete(TextEntry* entry)
{
}

// TODO(Blake): what to do with time parameter?
void FaustConnector::Process(double time)
{
   PROFILER(FaustConnector);

   // TODO(UI): when faust UI elements that act as visualizers are supported, we will need to treat them as valid outputs as well, and we shouldn't stop here if the dsp has any
   IAudioReceiver* target = GetTarget();
   if (target == nullptr)
      return;

   // before first reference to mDsp
   mDspDoubleBuf.SwitchBuffersIfNeeded();

   // TODO(Blake): decide if this is the best way to support the `enabled` button
   // IDEA: maybe we could do it with metadata attributes? (eg. have an attribute that says `disabledBehavior = bypass`)
   if (!mEnabled || !mDspDoubleBuf.GetCurrent().IsReady())
   {
      // Make the "enabled" button act as a bypass for faust programs that look like audio effects
      if (mDspDoubleBuf.GetCurrent().GetNumInputs() != 0 && GetBuffer()->NumActiveChannels() != 0)
      {
         SyncBuffers();
         for (int ch = 0; ch < GetBuffer()->NumActiveChannels(); ++ch)
         {
            Add(target->GetBuffer()->GetChannel(ch), GetBuffer()->GetChannel(ch), GetBuffer()->BufferSize());
            GetVizBuffer()->WriteChunk(GetBuffer()->GetChannel(ch), GetBuffer()->BufferSize(), ch);
         }
      }

      GetBuffer()->Reset();
      return;
   }

   { // setup faust buffers
      { // sync IO count
         int buf_count = MAX(mDspDoubleBuf.GetCurrent().GetNumInputs(), mDspDoubleBuf.GetCurrent().GetNumOutputs());
         SyncBuffers(buf_count);
      }

      { // input
         // enable to use the zero buffer for remaining mismatched channels
         int last_module_ch = MIN(FAUST_MAX_CHANNELS, GetBuffer()->NumActiveChannels());
         int last_dsp_ch = MIN(FAUST_MAX_CHANNELS, mDspDoubleBuf.GetCurrent().GetNumInputs());
         for (int ch = 0; ch < last_dsp_ch; ++ch)
         {
            if (ch < last_module_ch)
               mInChannels[ch] = GetBuffer()->GetChannel(ch);
            else
               mInChannels[ch] = gZeroBuffer;
         }
      }

      { // output
         int last_ch = MIN(mDspDoubleBuf.GetCurrent().GetNumOutputs(), MIN(FAUST_MAX_CHANNELS, target->GetBuffer()->NumActiveChannels()));

         gWorkChannelBuffer.SetNumActiveChannels(last_ch);

         for (int ch = 0; ch < last_ch; ++ch)
         {
            mOutChannels[ch] = gWorkChannelBuffer.GetChannel(ch);
         }

         {
            if (target->GetBuffer()->NumActiveChannels() != mDspDoubleBuf.GetCurrent().GetNumOutputs())
               return;
         }
      }
   }

   mDspDoubleBuf.GetCurrent().Process(time, mInChannels, mOutChannels);

   // copy to viz buffer
   for (int ch = 0; ch < gWorkChannelBuffer.NumActiveChannels(); ++ch)
   {
      Add(target->GetBuffer()->GetChannel(ch), gWorkChannelBuffer.GetChannel(ch), GetBuffer()->BufferSize());
      GetVizBuffer()->WriteChunk(gWorkChannelBuffer.GetChannel(ch), GetBuffer()->BufferSize(), ch);
   }

   GetBuffer()->Reset();
}