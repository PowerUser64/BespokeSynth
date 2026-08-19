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

#include "FaustConnector.h" // function declarations

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
   return true;
}

// TODO(Blake): Faust modules need to be initialized before we can say how much IO they have
bool FaustConnector::AcceptsAudio() { return true; }
bool FaustConnector::AcceptsNotes() { return false; }
bool FaustConnector::AcceptsPulses() { return false; }

// TODO(Blake): what to do with time parameter?
void FaustConnector::Process(double time)
{
   float** in_bufs_list = GetBuffer()->mBuffers;

   IAudioReceiver* target = GetTarget();
   float** out_bufs_list = nullptr;
   if (target)
      out_bufs_list = target->GetBuffer()->mBuffers;

   ofLog() << "out_bufs_list: " << out_bufs_list << " in_bufs_list: " << in_bufs_list;
   // TODO(Blake): simplify this logic
   // TODO(Blake): find how `NumActiveChannels` and `NumTotalChannels` are different
   if ((out_bufs_list && out_bufs_list[0] && mDsp.getNumOutputs() == target->GetBuffer()->NumActiveChannels()) && (mDsp.getNumInputs() == 0 || in_bufs_list && in_bufs_list[0] && mDsp.getNumInputs() == GetBuffer()->NumActiveChannels()))
      mDsp.compute(gBufferSize, in_bufs_list, out_bufs_list);
}

void FaustConnector::SetEnabled(bool enabled) { }

void FaustConnector::DrawModule() { }