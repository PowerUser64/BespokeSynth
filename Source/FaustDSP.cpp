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
// FaustDSP.cpp
//

#include "FaustDSP.h"

#include "OpenFrameworksPort.h"
#include "Profiler.h" // profiling
#include "SynthGlobals.h"
#include "faust/dsp/interpreter-dsp.h"
#include <cassert>
#include <string>

// A stand-in for cases when we need a generic faust dsp
const FaustDSP& gFaustDefaultProgram()
{
   static const FaustDSP instance("process = _, _;");
   return instance;
}

FaustDSP::FaustDSP()
: mDspString("process = _, _;") {
};

FaustDSP::~FaustDSP()
{
   delete mDsp;
   mDsp = 0;
   deleteInterpreterDSPFactory(mDspFactory);
   mDspFactory = 0;
};

// QUESTION:
// - how to deduplicate DSP's when editing? should we?
// - I think we shouldn't, and instead we should have save/load buttons

// DEMO
int dspIndex = -1;

FaustDSP::FaustDSP(std::string dspString)
: mFaustLibPath(ofToDataPath("scripts/faust/_stdlib"))
, mFaustFactoryArgv({ "-I", mFaustLibPath.c_str() })
, mDspString(dspString)
{
   UpdateDsp(dspString);
}

FaustDSP::FaustDSP(const FaustDSP& other)
: mDspString(other.mDspString)
, mFaustErrorString(other.mFaustErrorString)
, mFaustLibPath(other.mFaustLibPath)
, mFaustFactoryArgv(other.mFaustFactoryArgv)
{
   UpdateDsp(mDspString);
}

FaustDSP& FaustDSP::operator=(const FaustDSP& other)
{
   if (this != &other)
   {
      delete mDsp;
      mDsp = 0;
      deleteInterpreterDSPFactory(mDspFactory);
      mDspFactory = 0;

      mDspString = other.mDspString;
      mFaustErrorString = other.mFaustErrorString;
      mFaustLibPath = other.mFaustLibPath;
      mFaustFactoryArgv = { "-I", mFaustLibPath.c_str() };

      UpdateDsp(mDspString);
   }
   return *this;
}

void FaustDSP::UpdateDsp(std::string dspString)
{
   // TODO: figure out why this crashes (right now this is a leak)
   // Discard old DSP, if needed
   // if (mDspFactory != 0)
   // {
   //    deleteInterpreterDSPFactory(mDspFactory);
   //    mDspFactory = 0;
   // }
   // if (mDsp != 0)
   // {
   //    delete mDsp;
   //    mDsp = 0;
   // }

   mDspString = dspString;
   mFaustErrorString = "";
   mDspFactory = createInterpreterDSPFactoryFromString("FaustDSP", mDspString, mFaustFactoryArgv.size(), mFaustFactoryArgv.begin(), mFaustErrorString);

   // TODO: check `err` and display it on the module
   // TODO: remove the assert
   if (HasError() == false)
   {
      mDsp = mDspFactory->createDSPInstance();
      mDsp->init(gSampleRate);
      assert(IsReady());
   }
}

bool FaustDSP::HasError()
{
   bool hasFactory = mDspFactory != 0;
   bool hasFaustError = mFaustErrorString != "";

   bool ret = (hasFactory == false) || (hasFaustError == true);

   return ret;
}

inline bool FaustDSP::IsReady()
{
   bool hasDsp = mDsp != 0;

   bool _ret_is_ready = hasDsp && (HasError() == false);

   return _ret_is_ready;
}

// TODO(Blake): what to do with time parameter?
void FaustDSP::Process(double time, FaustChannelArray& mInChannels, FaustChannelArray& mOutChannels)
{
   PROFILER(FaustDSP);

   if (!IsReady())
      return;

   if (!mDsp)
      return;

   // TODO: sample-accurate sliders and checkboxes - ComputeSliders(i)
   mDsp->compute(gBufferSize, mInChannels.begin(), mOutChannels.begin());
}