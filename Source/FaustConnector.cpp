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
#include "Profiler.h" // profiling
#include "SynthGlobals.h"

FaustConnector::~FaustConnector() { };

FaustConnector::FaustConnector()
: IAudioProcessor(gBufferSize)
, IDrawableModule(120, 40)
{
   // TODO(Blake): Should we use init or instanceInit? We want to be able to spawn multiple of the module.
   mDsp.init(gSampleRate);
}

IDrawableModule* FaustConnector::Create()
{
   return new FaustConnector();
}

bool FaustConnector::IsEnabled() const
{
   return mEnabled;
}

// TODO(Blake): Faust modules need to be initialized before we can say how much IO they have
bool FaustConnector::AcceptsAudio() { return true; }
bool FaustConnector::AcceptsNotes() { return false; }
bool FaustConnector::AcceptsPulses() { return false; }

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
      if (mDsp.getNumInputs() != 0 && GetBuffer()->NumActiveChannels() != 0)
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

   int buf_count = MAX(mDsp.getNumInputs(), mDsp.getNumOutputs());
   SyncBuffers(buf_count);

   //  [x]   if input is null                                    -> always pass in the zero buffer
   //  [x]   if input.channel_count >= dsp.input_channel_count   -> pass in what we have to dsp
   //  [x]   if input.channel_count <  dsp.input_channel_count   -> pass in what we have to dsp
   //
   //  [x]   if output is null                                   -> skip processing
   //  [x]   if output.channel_count >= dsp.output_channel_count -> pass what we have to dsp
   //  [x]   if output.channel_count <  dsp.output_channel_count -> skip processing (?)

   // TODO: do more of this at module creation and/or use a dirty flag to mark it for updates here
   if (mDsp.getNumInputs() == 0 || GetBuffer()->NumActiveChannels() == 0)
   {
      in_chs = { gZeroBuffer, gZeroBuffer };
   }
   else
   {
      // enable to tile a low input channel count over the entire input range
#if false
      int last_ch = MIN(FAUST_MAX_CHANNELS, GetBuffer()->NumActiveChannels());
      for (int tch = 0; tch < MIN(FAUST_MAX_CHANNELS, mDsp.getNumInputs());)
         for (int ch = 0; ch < last_ch; ++ch, ++tch)
            in_chs[tch] = GetBuffer()->GetChannel(ch);
#else
      // enable to use the zero buffer for remaining mismatched channels
      int last_ch = MIN(FAUST_MAX_CHANNELS, GetBuffer()->NumActiveChannels());
      int ch = 0;
      for (; ch < last_ch; ++ch)
         in_chs[ch] = GetBuffer()->GetChannel(ch);

      int last_dsp_ch = MIN(FAUST_MAX_CHANNELS, mDsp.getNumInputs());
      if (last_ch < last_dsp_ch)
         for (; ch < last_dsp_ch; ++ch)
            in_chs[ch] = gZeroBuffer;
#endif
   }


   // if (target != target_previous)
   {
      int last_ch = MIN(mDsp.getNumOutputs(), MIN(FAUST_MAX_CHANNELS, target->GetBuffer()->NumActiveChannels()));

      gWorkChannelBuffer.SetNumActiveChannels(last_ch);

      for (int ch = 0; ch < last_ch; ++ch)
      {
         out_chs[ch] = gWorkChannelBuffer.GetChannel(ch);
      }

      {
         if (*target->GetBuffer()->GetAllChannels() == nullptr)
            return;
         if (target->GetBuffer()->NumActiveChannels() != mDsp.getNumOutputs())
            return;
      }
      target_previous = target;
   }

   mDsp.compute(gBufferSize, in_chs.begin(), out_chs.begin());

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

void FaustConnector::DrawModule() { }