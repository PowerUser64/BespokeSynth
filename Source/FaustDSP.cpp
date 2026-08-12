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
#include <cassert>
#include <string>

// A stand-in for cases when we need a generic faust dsp
const FaustDSP& gFaustDefaultProgram()
{
   static const FaustDSP instance("process = _, _;", false);
   return instance;
}

// TODO: clean up constructors and assignment op
FaustDSP::FaustDSP(std::string dspString, bool optimize)
: mFaustLibPath(ofToDataPath("scripts/faust/_stdlib"))
, mFaustFactoryArgv({ "-I", mFaustLibPath.c_str() })
, mDspString(dspString)
, mDsp(mFaustErrorString, dspString, optimize, mFaustFactoryArgv)
{
}

void FaustDSP::UpdateDsp(std::string dspString, bool optimize)
{
   mDsp.UpdateDsp(mFaustErrorString, dspString, optimize, mFaustFactoryArgv);

   if (HasError() == false)
      // at this point, we should be ready to call Process()
      assert(IsReady());
}

bool FaustDSP::HasError()
{
   bool hasFactory = mDsp.GetDspFactory() != 0;
   bool hasFaustError = mFaustErrorString != "";

   bool ret = (hasFactory == false) || (hasFaustError == true);

   return ret;
}

// TODO(Blake): what to do with time parameter?
void FaustDSP::Process(double time, FaustChannelArray& mInChannels, FaustChannelArray& mOutChannels)
{
   PROFILER(FaustDSP);

   // IsReady performs null check against mDsp
   if (!IsReady())
      return;

   // TODO: sample-accurate sliders and checkboxes - ComputeSliders(i)
   mDsp.GetDsp()->compute(gBufferSize, mInChannels.begin(), mOutChannels.begin());
}


////////////////////
//  DspContainer  //
////////////////////

// Move, then null out `other`
FaustDSP::DspContainer::DspContainer(DspContainer&& other)
: mDsp(other.mDsp)
, mDspFactory(other.mDspFactory)
, mIsInitialized(other.mIsInitialized)
, mIsLlvmOptimized(other.mIsLlvmOptimized)
{
   other.mDsp.interp = 0;
   other.mDspFactory.interp = 0;
   other.mIsInitialized = false;
}

void FaustDSP::DspContainer::UpdateDsp(std::string& faustErrorString, std::string& dspString, bool optimize, FaustArgv& argv)
{
   if (mIsInitialized)
      Delete();

   if (IsOptimized())
   { // use LLVM
      mDspFactory.llvm = createDSPFactoryFromString("FaustDSP", dspString, argv.size(), argv.begin(), "", faustErrorString);
      if (mDspFactory.llvm == nullptr)
         return;
      mDsp.llvm = mDspFactory.llvm->createDSPInstance();
   }
   else
   { // use interpreter
      mDspFactory.interp = createInterpreterDSPFactoryFromString("FaustDSP", dspString, argv.size(), argv.begin(), faustErrorString);
      if (mDspFactory.interp == nullptr)
         return;
      mDsp.interp = mDspFactory.interp->createDSPInstance();
   }

   mIsInitialized = true;

   if (GetDsp())
   { // this is required
      GetDsp()->init(gSampleRate);
   }
}

void FaustDSP::DspContainer::Delete()
{
   if (mIsInitialized)
   {
      if (IsOptimized())
      {
         delete mDsp.llvm;
         mDsp.llvm = 0;
         deleteDSPFactory(mDspFactory.llvm);
         mDspFactory.llvm = 0;
      }
      else
      {
         delete mDsp.interp;
         mDsp.interp = 0;
         deleteInterpreterDSPFactory(mDspFactory.interp);
         mDspFactory.interp = 0;
      }
   }
   mIsInitialized = false;
}