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
//  FaustConnector.h
//

#pragma once

// TODO(Blake): cleanup unused includes once factoring FaustDSP out of FaustConnector is done

#include "IAudioProcessor.h"
#include "PoliteDoubleBuffer.h"
#include <array>
#include "faust/dsp/interpreter-dsp.h"

// Faust includes (not directly used, TODO: figure out how to include these in the compiled program)
#include "faust/dsp/dsp.h"
#include "faust/gui/UI.h"

// TODO(Blake):
// This define exists because we need an array of pointers to channels for
// faust to process, and for memory management reasons, it's nice for its size
// to be known at compile time.
// TODO: check that the number of channels in the faust program is less than FAUST_MAX_CHANNELS (at compile time, in cmake)
#define FAUST_MAX_CHANNELS 2

typedef std::array<float*, FAUST_MAX_CHANNELS> FaustChannelArray;

class FaustDSP
{
public:
   // Module interface
   FaustDSP();
   FaustDSP(std::string dspString);
   FaustDSP(const FaustDSP& other);
   FaustDSP& operator=(const FaustDSP& that);
   virtual ~FaustDSP();

   // Process
   void Process(double time, FaustChannelArray& mInChannels, FaustChannelArray& mOutChannels);

   // DSP Lifecycle
   void UpdateDsp(std::string dspString);
   inline bool IsReady()
   {
      bool hasDsp = mDsp != 0;
      bool _ret_is_ready = hasDsp && (HasError() == false);
      return _ret_is_ready;
   }


   // Getters/Setters
   std::string GetDspString() { return mDspString; }
   std::string GetErrorString() { return mFaustErrorString; }
   bool HasError();
   int GetNumInputs()
   {
      assert(IsReady());
      return mDsp->getNumInputs();
   }
   int GetNumOutputs()
   {
      assert(IsReady());
      return mDsp->getNumOutputs();
   }
   void BuildUserInterface(UI* ui)
   {
      assert(IsReady());
      mDsp->buildUserInterface(ui);
   }

private:
   interpreter_dsp* mDsp = 0;
   interpreter_dsp_factory* mDspFactory = 0;

   std::string mDspString = "";
   std::string mFaustErrorString = "";

   std::string mFaustLibPath;
   std::array<const char*, 2> mFaustFactoryArgv;

   IAudioProcessor* mParentAudioProcessor = 0;
};

// A stand-in for cases where a generic faust dsp is needed
const FaustDSP& gFaustDefaultProgram();

// Helper class for switching between the two DSP's
typedef PoliteDoubleBuffer<FaustDSP> DspPair;