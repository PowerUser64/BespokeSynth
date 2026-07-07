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
   // sine-advanced-stereo-params.dsp
   R"(import("stdfaust.lib"); f = hslider("freq", 220, 55, 880, 0.01); process = ((_ + 1) * os.osc(f)) * 0.5, ((_ + 1) * os.osc(f)) * 0.5 : _ * 1/2, _ * 1/2;)",
   // panning-sine.dsp
   R"(import("stdfaust.lib"); lfo = os.osc(2); lfo2 = cos(os.sawtooth(1)/2+1); sig = os.osc(220); process = sig * lfo2, sig * lfo;)",
   // add-params.dsp
   R"(s0 = hslider("s0", 0, -1, 1, 0.01); s1 = hslider("s1", 0, -1, 1, 0.01); s2 = hslider("s2", 0, -1, 1, 0.01); s3 = hslider("s3", 0, -1, 1, 0.01); s4 = hslider("s4", 0, -1, 1, 0.01); s5 = hslider("s5", 0, -1, 1, 0.01); s6 = hslider("s6", 0, -1, 1, 0.01); s7 = hslider("s7", 0, -1, 1, 0.01); s8 = hslider("s8", 0, -1, 1, 0.01); s9 = hslider("s9", 0, -1, 1, 0.01); add = _ + s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9; process = add, add;)",
   // all-ui-elements.dsp
   R"(import("stdfaust.lib"); b = button("button"); c = checkbox("checkbox"); s = hslider("hslider", 0, 0, 1, 0.1); S = vslider("vslider", 0, 0, 1, 0.1); n = nentry("numentry", 0, 0, 1, 0.1); g = hbargraph("hbargraph", -1, 1); G = vbargraph("vbargraph", -1, 1); all = b*c*s*S*n : g : G; process = _ * all, _ * all;)",
};

// filesystem cache
std::vector<std::string> cachedPrograms(programs.size());

FaustConnector::FaustConnector()
: IAudioProcessor(gBufferSize)
, IDrawableModule(120, 10)
, mDspUi(this, this, this)
, mFaustLibPath(ofToDataPath("scripts/faust/_stdlib"))
, mFaustFactoryArgv({ "-I", mFaustLibPath.c_str() })
{
   dspIndex = (dspIndex + 1) % programs.size();

   assert(programs.size() == cachedPrograms.size());

   // DEMO: pick a dsp from the list of programs
   mDspString = programs[dspIndex];

   std::string err;
   if (cachedPrograms[dspIndex] != "")
   {
      ofLog() << "FaustConnector: Cache hit: Using cached bitcode";
      mDspFactory = readInterpreterDSPFactoryFromBitcode(cachedPrograms[dspIndex], err);
   }
   else
   {
      ofLog() << "FaustConnector: Cache miss: Compiling DSP factory";
      mDspFactory = createInterpreterDSPFactoryFromString("faustconnector", mDspString, mFaustFactoryArgv.size(), mFaustFactoryArgv.begin(), err);
   }

   // TODO: check `err` and display it on the module
   // TODO: remove the assert
   assert(mDspFactory != 0);
   mDsp = mDspFactory->createDSPInstance();
   mDsp->init(gSampleRate);

   cachedPrograms[dspIndex] = writeInterpreterDSPFactoryToBitcode(mDspFactory);
}

// TODO(Blake): Faust modules need to be initialized before we can say how much IO they have
bool FaustConnector::AcceptsAudio() { return true; }
bool FaustConnector::AcceptsNotes() { return false; }
bool FaustConnector::AcceptsPulses() { return false; }

IDrawableModule* FaustConnector::Create()
{
   FaustConnector* ret = new FaustConnector();
   assert(ret->mDsp->getNumOutputs() <= FAUST_MAX_CHANNELS);
   assert(ret->mDsp->getNumInputs() <= FAUST_MAX_CHANNELS);
   return ret;
}

void FaustConnector::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mDsp->buildUserInterface(&mDspUi);
}

bool FaustConnector::IsEnabled() const
{
   return mEnabled;
}

void FaustConnector::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   mDspUi.Impl_DrawControls();
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

   // TODO(Blake): decide if this is the best way to support the `enabled` button
   // IDEA: maybe we could do it with metadata attributes? (eg. have an attribute that says `disabledBehavior = bypass`)
   if (!mEnabled)
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