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
#include <cassert>

template <typename T>
class PoliteDoubleBuffer
{
private: // Data members
   // The Buffers
   T mDoubleBuffer[2];

   // Core internal data type
   typedef unsigned char Byte;

   // Data and locks
   std::atomic_uchar mBitFlags = 0;
   const Byte mIndexFlag = 0b001; // the current index into the double-buffer
   const Byte mShouldSwitchFlag = 0b010; // whether we should flip index to point at the other buffer
   const Byte mAudioThreadMutexFlag = 0b100; // whether it's safe to switch

public: // Public API
   PoliteDoubleBuffer(T current, T other)
   : mDoubleBuffer{ current, other }
   { }


   //
   // UI thread API
   //

   // Get the buffer that isn't in front.
   // Caller: call ONLY from non-audio thread
   // Thread-safety: this function assumes it isn't called from more than one thread
   inline T& GetBackBuffer()
   {
      assert(!IsAudioThread());
      EnsureNotWaitingForSwitch();
      return mDoubleBuffer[!GetIndex()];
   }
   // Say we should switch buffers
   // Caller: call ONLY from non-audio thread
   // Thread-safety: this function assumes it isn't called from more than one thread
   inline void SwitchWhenReady()
   {
      assert(!IsAudioThread());
      // if we have a switch requested already, this means the audio thread is likely paused, and it's fine to lock it
      if (ShouldSwitch())
         // TODO: fix slight TOCTOU here - we need to capture the mutex AND the ShouldSwitch lock in order to be certain we can call ForceSwitchNowAndBlockAudioThread
         ForceSwitchNowAndBlockAudioThread();
      else
         mBitFlags.fetch_or(mShouldSwitchFlag, std::memory_order_release);
   }
   // Perform a switch from the non-audio thread. Only use if you need to call switch and you know the audio thread is stopped.
   // Caller: call ONLY from non-audio thread
   // Thread-safety: this function assumes it isn't called from more than one thread
   inline void ForceSwitchNowAndBlockAudioThread()
   {
      assert(!IsAudioThread());
      assert(ShouldSwitch()); // you should only call this if you have requested a switch

      AcquireAudioThreadMutex();
      Unsafe_DoBufferSwitch();
      ReleaseAudioThreadMutex();
   }


   //
   // Audio thread API
   //

   // Get current buffer.
   // Caller: call ONLY from audio thread
   inline T& GetFrontBuffer()
   {
      assert(IsAudioThread());
      return mDoubleBuffer[GetIndex()];
   }
   // Acquire the audio thread mutex and switch buffers if needed.
   // Caller: call ONLY from audio thread
   inline void BeginAudioThread()
   {
      assert(IsAudioThread());
      AcquireAudioThreadMutex();
      SwitchBuffersIfNeeded();
   }
   // Release the audio thread mutex.
   // Caller: call ONLY from audio thread
   inline void EndAudioThread()
   {
      assert(IsAudioThread());
      ReleaseAudioThreadMutex();
   }

private: // Private methods
   //
   // Member functions
   //

   // Switch buffers if we should (NOTE: call ONLY from audio thread)
   inline void SwitchBuffersIfNeeded()
   {
      assert(IsAudioThread());
      if (ShouldSwitch())
      {
         Unsafe_DoBufferSwitch();
      }
   }

   // Acquire the audio thread mutex
   inline void AcquireAudioThreadMutex()
   {
      assert(!AudioThreadMutexIsAcquired());
      Byte expected;

      // wait until mutex is free
      while ((expected = mBitFlags.load(std::memory_order_relaxed)) & mAudioThreadMutexFlag)
         ;

      // acquire the mutex
      while (!mBitFlags.compare_exchange_weak(expected,
                                              expected | mAudioThreadMutexFlag,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed))
      {
         // if the mutex was acquired again, wait until it's free again
         while ((expected = mBitFlags.load(std::memory_order_relaxed)) & mAudioThreadMutexFlag)
            ;
      };
   }

   // Small helper function to perform the buffer switch action. Expects you've done all the work to make this safe (checking locks).
   inline void Unsafe_DoBufferSwitch()
   {
      // slight safety check
      assert(ShouldSwitch() && AudioThreadMutexIsAcquired());
      // flip index
      mBitFlags.fetch_xor(mIndexFlag, std::memory_order_release);
      // clear ShouldSwitch
      mBitFlags.fetch_and(~mShouldSwitchFlag, std::memory_order_release);
   }

   // Release the audio thread mutex
   inline void ReleaseAudioThreadMutex()
   {
      assert(AudioThreadMutexIsAcquired());
      mBitFlags.fetch_and(~mAudioThreadMutexFlag, std::memory_order_release);
   }

   // Check bitflags
   inline bool ShouldSwitch()
   {
      return (mBitFlags.load(std::memory_order_acquire) & mShouldSwitchFlag) != 0;
   }
   inline bool AudioThreadMutexIsAcquired()
   {
      return (mBitFlags.load(std::memory_order_acquire) & mAudioThreadMutexFlag) != 0;
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
      }
   }
};