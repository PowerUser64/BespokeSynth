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

#include "PoliteDoubleBuffer.h"
#include <array>
#include <cassert>
#include "faust/dsp/interpreter-dsp.h"
#include "faust/dsp/llvm-dsp.h"

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
   FaustDSP(const FaustDSP&) = delete;
   FaustDSP(FaustDSP&&) noexcept = default;
   FaustDSP& operator=(const FaustDSP&) = delete;
   FaustDSP& operator=(FaustDSP&&) = delete;

   // Module interface
   FaustDSP(std::string dspString, bool optimize);
   ~FaustDSP() = default;

   // Process
   void Process(double time, FaustChannelArray& mInChannels, FaustChannelArray& mOutChannels);

   // DSP Lifecycle
   void UpdateDsp(std::string dspString, bool optimize);
   inline bool IsReady()
   {
      bool hasDsp = mDsp.GetDsp() != 0;
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
      void UpdateDsp(std::string& faustErrorString, std::string& dspString, bool optimize, FaustArgv& argv);
      bool IsOptimized() const { return mIsLlvmOptimized; }

      DspContainer(const DspContainer&) = delete;
      DspContainer(DspContainer&&) noexcept;
      DspContainer& operator=(const DspContainer&) = delete;
      DspContainer& operator=(DspContainer&&) = delete;

      // default constructor: nothing is initialized
      DspContainer()
      {
         mNeedsDspCleanup = false;
      }
      DspContainer(std::string& faustErrorString, std::string& dspString, bool optimize, FaustArgv& argv)
      : mIsLlvmOptimized(optimize)
      {
         UpdateDsp(faustErrorString, dspString, optimize, argv);
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
         if (IsOptimized()) return mDsp.llvm;
         else               return mDsp.interp;
      }
      inline dsp_factory* GetDspFactory() const {
         assert(mNeedsDspCleanup);
         if (IsOptimized()) return mDspFactory.llvm;
         else               return mDspFactory.interp;
      }
      // clang-format on

   private:
      // Delete the factory and dsp
      void Delete();

      union DspTypeUnion {
         interpreter_dsp* interp;
         llvm_dsp* llvm;
      };
      union DspFactoryTypeUnion {
         interpreter_dsp_factory* interp;
         llvm_dsp_factory* llvm;
      };

      DspTypeUnion mDsp{ 0 };
      DspFactoryTypeUnion mDspFactory{ 0 };
      // have we attempted to create the dsp?
      bool mNeedsDspCleanup = false;
      // should we use the LLVM version?
      bool mIsLlvmOptimized = false;
   };

   DspContainer mDsp;

   std::string mDspString = "";
   std::string mFaustErrorString = "";

   std::string mFaustLibPath;
   FaustArgv mFaustFactoryArgv;
};

// Helper class for switching between the two DSP's
typedef PoliteDoubleBuffer<FaustDSP> DspPair;