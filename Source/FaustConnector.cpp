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
#include "IAudioReceiver.h"
#include "ModularSynth.h" // save/load
#include "OpenFrameworksPort.h"
#include "Profiler.h" // profiling
#include "SynthGlobals.h"
#include "faust/dsp/interpreter-dsp.h"
#include <cassert>
#include <string>

FaustConnector::~FaustConnector()
{
   delete mDsp;
};

// QUESTION:
// - how to deduplicate DSP's when editing? should we?
// - I think we shouldn't, and instead we should have save/load buttons

// DEMO
int dspIndex = -1;

// filesystem
std::vector<std::string> programs = {
   // error.dsp
   // R"(process = _)",
   // sine-advanced-stereo-params.dsp
   R"(import("stdfaust.lib");)"
   "\n"
   R"(f = hslider("freq", 220, 55, 880, 0.01);)"
   "\n"
   R"(process = ((_ + 1) * os.osc(f)) * 0.5,)"
   "\n"
   R"(          ((_ + 1) * os.osc(f)))"
   "\n"
   R"(          : _ * 1/2,)"
   "\n"
   R"(            _ * 1/2;)",
   // panning-sine.dsp
   R"(import("stdfaust.lib");)"
   "\n"
   R"(lfo = os.osc(2);)"
   "\n"
   R"(lfo2 = cos(os.sawtooth(1)/2+1);)"
   "\n"
   R"(sig = os.osc(220);)"
   "\n"
   R"(process = sig * lfo2, sig * lfo;)",
   // add-params.dsp
   R"(s0 = hslider("s0", 0, -1, 1, 0.01);)"
   "\n"
   R"(s1 = hslider("s1", 0, -1, 1, 0.01);)"
   "\n"
   R"(s2 = hslider("s2", 0, -1, 1, 0.01);)"
   "\n"
   R"(s3 = hslider("s3", 0, -1, 1, 0.01);)"
   "\n"
   R"(s4 = hslider("s4", 0, -1, 1, 0.01);)"
   "\n"
   R"(s5 = hslider("s5", 0, -1, 1, 0.01);)"
   "\n"
   R"(s6 = hslider("s6", 0, -1, 1, 0.01); )"
   "\n"
   R"(s7 = hslider("s7", 0, -1, 1, 0.01); )"
   "\n"
   R"(s8 = hslider("s8", 0, -1, 1, 0.01); )"
   "\n"
   R"(s9 = hslider("s9", 0, -1, 1, 0.01); )"
   "\n"
   R"(add = _ + s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9; )"
   "\n"
   R"(process = add, add;)",
   // all-ui-elements.dsp
   R"(import("stdfaust.lib"); )"
   "\n"
   R"(b = button("button"); )"
   "\n"
   R"(c = checkbox("checkbox"); )"
   "\n"
   R"(s = hslider("hslider", 0, 0, 1, 0.1); )"
   "\n"
   R"(S = vslider("vslider", 0, 0, 1, 0.1); )"
   "\n"
   R"(n = nentry("numentry", 0, 0, 1, 0.1); )"
   "\n"
   R"(g = hbargraph("hbargraph", -1, 1); )"
   "\n"
   R"(G = vbargraph("vbargraph", -1, 1); )"
   "\n"
   R"(all = b*c*s*S*n : g : G; )"
   "\n"
   R"(process = _ * all, _ * all;)",
};

FaustConnector::FaustConnector()
: IAudioProcessor(gBufferSize)
, IDrawableModule(120, 10)
, mDspUi(new FaustUI(this, this, this))
, mFaustLibPath(ofToDataPath("scripts/faust/_stdlib"))
, mFaustFactoryArgv({ "-I", mFaustLibPath.c_str() })
{
   dspIndex = (dspIndex + 1) % programs.size();
   mDspString = programs[dspIndex];

   CompileFaustDsp();
}

// TODO(Blake): Faust modules need to be initialized before we can say how much IO they have
bool FaustConnector::AcceptsAudio() { return true; }
bool FaustConnector::AcceptsNotes() { return false; }
bool FaustConnector::AcceptsPulses() { return false; }

IDrawableModule* FaustConnector::Create()
{
   FaustConnector* ret = new FaustConnector();
   if (ret->mDsp)
   {
      assert(ret->mDsp->getNumOutputs() <= FAUST_MAX_CHANNELS);
      assert(ret->mDsp->getNumInputs() <= FAUST_MAX_CHANNELS);
   }
   return ret;
}

void FaustConnector::CreateUIControls()
{
   IDrawableModule::CreateUIControls();

   mDspEditorBox = new CodeEntry(this, "dsp_editor", 3, 100, 300, 300);
}

void FaustConnector::CompileFaustDsp()
{
   if (mDspFactory != nullptr)
   {
      deleteInterpreterDSPFactory(mDspFactory);
   }

   mDspFactory = createInterpreterDSPFactoryFromString("faustconnector", mDspString, mFaustFactoryArgv.size(), mFaustFactoryArgv.begin(), mFaustErrorStr);

   // TODO: check `err` and display it on the module
   // TODO: remove the assert
   if (HasFaustError() == false)
   {
      interpreter_dsp* newDsp = mDspFactory->createDSPInstance();
      newDsp->init(gSampleRate);
      if (newDsp)
      {
         // QUESTION: is making sliders outside CreateUIControls bad?
         if (mDsp)
         {
            assert(mDspUi);
            delete mDsp;
            delete mDspUi;
            // TODO: how to reuse memory from mDspUi?
            mDspUi = new FaustUI(this, this, this);
         }
         mDsp = newDsp;
         mDsp->buildUserInterface(mDspUi);
      }
   }
   else
      HandleFaustError();
}

void FaustConnector::HandleFaustError()
{
   if (HasFaustError() == false)
      return;

   ofLog() << "Faust error:" << '\n'
           << mFaustErrorStr;
}

bool FaustConnector::HasFaustError()
{
   bool hasError = mDspFactory == 0 && mFaustErrorStr != "";
   return hasError;
}

bool FaustConnector::IsEnabled() const
{
   return mEnabled;
}

void FaustConnector::DrawModule()
{
   ofLog() << "DEBUGPRINT[1]: " << __FILE__ << ":" << __LINE__ << " (after void FaustConnector::DrawModule())";
   if (Minimized() || IsVisible() == false)
      return;
   ofLog() << "DEBUGPRINT[2]: " << __FILE__ << ":" << __LINE__ << " (after return;)";

   mDspUi->Impl_DrawControls();
   ofLog() << "DEBUGPRINT[3]: " << __FILE__ << ":" << __LINE__ << " (after mDspUi->Impl_DrawControls();)";

   mDspEditorBox->SetShowing(mEditMode);
   ofLog() << "DEBUGPRINT[5]: " << __FILE__ << ":" << __LINE__ << ": mEditMode=" << mEditMode;
   if (mEditMode)
   {
      ofLog() << "DEBUGPRINT[6]: " << __FILE__ << ":" << __LINE__ << " (after if (mEditMode))";
      mDspEditorBox->SetPosition(mDspUi->GetUiLeftEdgeOffset(), mDspUi->GetUiBottomEdgeOffset());
      ofLog() << "DEBUGPRINT[7]: " << __FILE__ << ":" << __LINE__ << " (after mDspEditorBox->SetPosition(mDspUi->GetUi…)";

      ofRectangle rect = mDspEditorBox->GetRect(K(local));
      mWidth = MAX(mWidth, rect.x + rect.width + 3);
      mHeight = rect.y + rect.height + 3;

      mDspEditorBox->Draw();
   }
   ofLog() << "DEBUGPRINT[11]: " << __FILE__ << ":" << __LINE__ << " (after mDspEditorBox->Draw();)";
}

void FaustConnector::KeyPressed(int key, bool isRepeat)
{
   IDrawableModule::KeyPressed(key, isRepeat);

   if (gHoveredModule == this)
   {
      if (key == OF_KEY_RETURN && GetKeyModifiers() == kModifier_Shift)
      {
         if (mEditMode == false)
         {
            mDspEditorBox->SetText(mDspString);
            mDspEditorBox->ResetScroll();
            IKeyboardFocusListener::SetActiveKeyboardFocus(mDspEditorBox);
            mEditMode = true;
         }
      }
   }
}

void FaustConnector::ExecuteCode()
{
   mDspString = mDspEditorBox->GetText(true);
   CompileFaustDsp();
   HandleFaustError();
   mEditMode = false;
}

void FaustConnector::CheckboxUpdated(Checkbox* checkbox, double time)
{
   mDspUi->Impl_CheckboxUpdate(checkbox, time);
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

   if (mDsp == nullptr)
      return;

   // TODO(Blake): decide if this is the best way to support the `enabled` button
   // IDEA: maybe we could do it with metadata attributes? (eg. have an attribute that says `disabledBehavior = bypass`)
   if (!mEnabled || HasFaustError())
   {
      // Make the "enabled" button act as a bypass for faust programs that look like audio effects
      if (mDsp->getNumInputs() != 0 && GetBuffer()->NumActiveChannels() != 0)
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

   int buf_count = MAX(mDsp->getNumInputs(), mDsp->getNumOutputs());
   SyncBuffers(buf_count);

   if (mDsp->getNumInputs() == 0 || GetBuffer()->NumActiveChannels() == 0)
   {
      mInChannels = { gZeroBuffer, gZeroBuffer };
   }
   else
   {
      // enable to use the zero buffer for remaining mismatched channels
      int last_module_ch = MIN(FAUST_MAX_CHANNELS, GetBuffer()->NumActiveChannels());
      int last_dsp_ch = MIN(FAUST_MAX_CHANNELS, mDsp->getNumInputs());
      for (int ch = 0; ch < last_dsp_ch; ++ch)
      {
         if (ch < last_module_ch)
            mInChannels[ch] = GetBuffer()->GetChannel(ch);
         else
            mInChannels[ch] = gZeroBuffer;
      }
   }


   {
      int last_ch = MIN(mDsp->getNumOutputs(), MIN(FAUST_MAX_CHANNELS, target->GetBuffer()->NumActiveChannels()));

      gWorkChannelBuffer.SetNumActiveChannels(last_ch);

      for (int ch = 0; ch < last_ch; ++ch)
      {
         mOutChannels[ch] = gWorkChannelBuffer.GetChannel(ch);
      }

      {
         if (target->GetBuffer()->NumActiveChannels() != mDsp->getNumOutputs())
            return;
      }
   }

   mDsp->compute(gBufferSize, mInChannels.begin(), mOutChannels.begin());

   for (int ch = 0; ch < gWorkChannelBuffer.NumActiveChannels(); ++ch)
   {
      Add(target->GetBuffer()->GetChannel(ch), gWorkChannelBuffer.GetChannel(ch), GetBuffer()->BufferSize());
      GetVizBuffer()->WriteChunk(gWorkChannelBuffer.GetChannel(ch), GetBuffer()->BufferSize(), ch);
   }

   GetBuffer()->Reset();
}

void FaustConnector::SetEnabled(bool enabled)
{
   mEnabled = enabled;
}