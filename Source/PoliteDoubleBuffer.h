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
// PoliteDoubleBuffer.h
//


#pragma once

#include "SynthGlobals.h"
#include "juce_core/juce_core.h"
#include <atomic>
#include <utility>

template <typename T>
class PoliteDoubleBuffer
{
   typedef unsigned char Byte;

public:
   PoliteDoubleBuffer(T(current), T other)
   : mDoubleBuffer{ std::move(current), std::move(other) }
   {}


   //
   // UI thread API
   //

   // Say we should switch buffers (NOTE: call ONLY from non-audio thread)
   // (NOTE: this function assumes it isn't called from more than one thread)
   inline void SwitchWhenReady()
   {
      assert(!IsAudioThread());
      EnsureNotWaitingForSwitch();
      mBitFlags.fetch_or(mSwitchReadyFlag, std::memory_order_release);
   }
   // Get the buffer that isn't in front (NOTE: call ONLY from the non-audio thread)
   // (NOTE: this function assumes it isn't called from more than one thread)
   inline T& GetBackBuffer()
   {
      assert(!IsAudioThread());
      EnsureNotWaitingForSwitch();
      return mDoubleBuffer[!GetIndex()];
   }


   //
   // Audio thread API
   //

   // Get current buffer (NOTE: call ONLY from audio thread)
   inline T& GetFrontBuffer()
   {
      assert(IsAudioThread());
      return mDoubleBuffer[GetIndex()];
   }
   // Switch buffers if we should (NOTE: call ONLY from audio thread)
   inline void SwitchBuffersIfNeeded()
   {
      assert(IsAudioThread());
      if (ShouldSwitch())
      {
         // flip index
         mBitFlags.fetch_xor(mIndexFlag, std::memory_order_release);
         // clear ShouldSwitch
         mBitFlags.fetch_and(~mSwitchReadyFlag, std::memory_order_release);
      }
   }

private:
   // The buffer
   T mDoubleBuffer[2];

   std::atomic_uchar mBitFlags = 0;
   const Byte mIndexFlag = 0b01;
   const Byte mSwitchReadyFlag = 0b10;

   // Check bitflags
   inline bool ShouldSwitch()
   {
      return (mBitFlags.load(std::memory_order_acquire) & mSwitchReadyFlag) != 0;
   }
   inline Byte GetIndex()
   {
      Byte index = mBitFlags.load(std::memory_order_acquire) & mIndexFlag;
      return index;
   }

   // If a switch is supposed to take place, block until it's done
   // If the audio thread is paused
   inline void EnsureNotWaitingForSwitch()
   {
      // spinlock (TODO: store+execute remainder of the non-RT function)
      while (ShouldSwitch())
      {
         juce::Thread::yield();
      };
   }
};