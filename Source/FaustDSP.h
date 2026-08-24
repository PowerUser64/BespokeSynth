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

#ifndef BESPOKE_FAUST_USE_LLVM
#define BESPOKE_FAUST_USE_LLVM false
#endif

#include "PoliteDoubleBuffer.h"
#include <array>
#include <cassert>

#if BESPOKE_FAUST_USE_LLVM
#include "faust/dsp/llvm-dsp.h"
#else
#include "faust/dsp/interpreter-dsp.h"
#endif

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
typedef std::vector<const char*> FaustArgv;

class FaustDSP
{
public:
   struct FaustIR
   {
      bool mIrIsInitialized = false;
      bool mIsLlvmOptimized = BESPOKE_FAUST_USE_LLVM;
      std::string mLlvmTarget = "";
      std::string mDspString = "";
      std::string mIr = "";
   };

   FaustDSP(const FaustDSP&) = delete;
   FaustDSP(FaustDSP&&) noexcept = default;
   FaustDSP& operator=(const FaustDSP&) = delete;
   FaustDSP& operator=(FaustDSP&&) = delete;

   // Module interface
   FaustDSP(std::string dspString);
   ~FaustDSP() = default;

   // Process
   void Process(double time, FaustChannelArray& mInChannels, FaustChannelArray& mOutChannels);

   // DSP Lifecycle
   void UpdateDspFromString(std::string dspString);
   void UpdateDspFromIr(FaustIR& ir) { mDsp.UpdateDspFromIr(mFaustErrorString, ir, mFaustFactoryArgv); }
   inline bool IsReady()
   {
      bool hasDsp = mDsp.GetDsp() != 0;
      bool _ret_is_ready = hasDsp && (HasError() == false);
      return _ret_is_ready;
   }

   // Getters/Setters
   std::string GetDspString() { return mDspString; }
   FaustIR GetDspIr()
   {
      return {
         .mIrIsInitialized = true,
         .mIsLlvmOptimized = BESPOKE_FAUST_USE_LLVM,
         .mLlvmTarget = mDsp.GetLlvmTarget(),
         .mDspString = mDspString,
         .mIr = mDsp.GetDspIr(),
      };
   }
   std::string GetErrorString() { return mFaustErrorString; }
   bool HasError();
   int GetNumInputs()
   {
      assert(IsReady());
      return mDsp.GetDsp()->getNumInputs();
   }
   int GetNumOutputs()
   {
      assert(IsReady());
      return mDsp.GetDsp()->getNumOutputs();
   }
   void BuildUserInterface(UI* ui)
   {
      assert(IsReady());
      mDsp.GetDsp()->buildUserInterface(ui);
   }

private:
   // Manages things that require managing llvm/interpreter dsp's separately
   class DspContainer
   {
   public:
      // Update the dsp
      void UpdateDsp(std::string& faustErrorString, std::string& dspString, FaustArgv& argv);
      void UpdateDspFromIr(std::string& faustErrorString, FaustDSP::FaustIR& ir, FaustArgv& argv);

      DspContainer(const DspContainer&) = delete;
      DspContainer(DspContainer&&) noexcept;
      DspContainer& operator=(const DspContainer&) = delete;
      DspContainer& operator=(DspContainer&&) = delete;

      // default constructor: nothing is initialized
      DspContainer()
      {
         mNeedsDspCleanup = false;
      }
      DspContainer(std::string& faustErrorString, std::string& dspString, FaustArgv& argv)
      {
         UpdateDsp(faustErrorString, dspString, argv);
      }
      ~DspContainer()
      {
         Delete();
      }

      // TODO: do these functions need to return the correct pointer, or is either one always valid?
      // these short functions don't deserve 7 lines each, just makes it hard to read them
      // clang-format off
      inline dsp* GetDsp() const {
         assert(mNeedsDspCleanup);
         return mDsp;
      }
      inline dsp_factory* GetDspFactory() const {
         assert(mNeedsDspCleanup);
         return mDspFactory;
      }
      inline std::string GetDspIr() const {
         assert(mNeedsDspCleanup);
         #if BESPOKE_FAUST_USE_LLVM
            return writeDSPFactoryToMachine(mDspFactory, GetLlvmTarget());
         #else
            return writeInterpreterDSPFactoryToBitcode(mDspFactory);
         #endif
      }
      inline std::string GetLlvmTarget() const {
         assert(mNeedsDspCleanup);
         #if BESPOKE_FAUST_USE_LLVM
            return ""; // TODO: return the llvm target according to what the build machine's target name is
         #else
            return "";
         #endif
      }
      // clang-format on

   private:
      // Delete the factory and dsp
      void Delete();

#if BESPOKE_FAUST_USE_LLVM
      typedef llvm_dsp* DspType;
      typedef llvm_dsp_factory* DspFactoryType;
#else
      typedef interpreter_dsp* DspType;
      typedef interpreter_dsp_factory* DspFactoryType;
#endif

      DspType mDsp{ 0 };
      DspFactoryType mDspFactory{ 0 };
      // have we attempted to create the dsp?
      bool mNeedsDspCleanup = false;
   };

   DspContainer mDsp;

   std::string mDspString = "";
   std::string mFaustErrorString = "";

   std::string mFaustLibPath;
   FaustArgv mFaustFactoryArgv;
};

std::string gDefaultFaustProgram();

// Helper class for switching between the two DSP's
typedef PoliteDoubleBuffer<FaustDSP> DspPair;