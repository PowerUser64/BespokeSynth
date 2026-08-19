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

std::string gDefaultFaustProgram() { return "process = _, _;"; }

// TODO: clean up constructors and assignment op
FaustDSP::FaustDSP(std::string dspString)
{
   // TODO: scan faust script directory for other directories that were prefixed with an underscore and make them all libraries
   mFaustLibPath = ofToDataPath("scripts/faust/_stdlib");
   // TODO: set mFaustFactoryArgv size before pushing all lib paths
   mFaustFactoryArgv.push_back("-I");
   mFaustFactoryArgv.push_back(mFaustLibPath.c_str());
   mDsp.UpdateDsp(mFaustErrorString, dspString, mFaustFactoryArgv);
}

void FaustDSP::UpdateDspFromString(std::string dspString)
{
   mDspString = dspString;
   mDsp.UpdateDsp(mFaustErrorString, dspString, mFaustFactoryArgv);

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
FaustDSP::DspContainer::DspContainer(DspContainer&& other) noexcept
: mDsp(other.mDsp)
, mDspFactory(other.mDspFactory)
, mNeedsDspCleanup(other.mNeedsDspCleanup)
{
   other.mDsp = 0;
   other.mDspFactory = 0;
   other.mNeedsDspCleanup = false;
}

void FaustDSP::DspContainer::UpdateDspFromIr(std::string& faustErrorString, FaustDSP::FaustIR& ir, FaustArgv& argv)
{
   if (mNeedsDspCleanup)
      Delete();

   mNeedsDspCleanup = true;

   if (ir.mIsLlvmOptimized == BESPOKE_FAUST_USE_LLVM)
   {
#if BESPOKE_FAUST_USE_LLVM
      mDspFactory = readDSPFactoryFromMachine(ir.mIr, ir.mLlvmTarget, faustErrorString);
#else
      mDspFactory = readInterpreterDSPFactoryFromBitcode(ir.mIr, faustErrorString);
#endif
   }
   else
   {
      // if we can't use the IR because it doesn't match our backend, compile it instead
      ofLog() << "Recompiling faust code due to save data backend mismatch";
      UpdateDsp(faustErrorString, ir.mDspString, argv);
   }


   if (mDspFactory == nullptr)
      return;
   mDsp = mDspFactory->createDSPInstance();

   if (GetDsp())
   {
      GetDsp()->init(gSampleRate);
   }
}

void FaustDSP::DspContainer::UpdateDsp(std::string& faustErrorString, std::string& dspString, FaustArgv& argv)
{
   if (mNeedsDspCleanup)
      Delete();

   mNeedsDspCleanup = true;

#if BESPOKE_FAUST_USE_LLVM
   mDspFactory = createDSPFactoryFromString("FaustDSP", dspString, argv.size(), const_cast<const char**>(argv.data()), "", faustErrorString);
#else
   mDspFactory = createInterpreterDSPFactoryFromString("FaustDSP", dspString, argv.size(), const_cast<const char**>(argv.data()), faustErrorString);
#endif
   if (mDspFactory == nullptr)
      return;
   mDsp = mDspFactory->createDSPInstance();

   if (GetDsp())
   {
      GetDsp()->init(gSampleRate);
   }
}

void FaustDSP::DspContainer::Delete()
{
   if (mNeedsDspCleanup)
   {

      if (mDsp)
      {
         delete mDsp;
         mDsp = 0;
      }
      if (mDspFactory)
      {
#if BESPOKE_FAUST_USE_LLVM
         deleteDSPFactory(mDspFactory);
#else
         deleteInterpreterDSPFactory(mDspFactory);
#endif
         mDspFactory = 0;
      }
   }
   mNeedsDspCleanup = false;
}