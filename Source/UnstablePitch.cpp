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
/*
  ==============================================================================

    UnstablePitch.cpp
    Created: 2 Mar 2021 7:48:14pm
    Author:  Ryan Challinor

  ==============================================================================
*/

#include "UnstablePitch.h"
#include "OpenFrameworksPort.h"
#include "ModularSynth.h"
#include "UIControlMacros.h"

UnstablePitch::UnstablePitch()
{
   for (int voice = 0; voice < kNumVoices; ++voice)
      mModulation.GetPitchBend(voice)->CreateBuffer();
}

void UnstablePitch::Init()
{
   IDrawableModule::Init();

   TheTransport->AddAudioPoller(this);
}

UnstablePitch::~UnstablePitch()
{
   TheTransport->RemoveAudioPoller(this);
}

void UnstablePitch::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   UIBLOCK(3, 40, 140);
   FLOATSLIDER(mAmountSlider, "amount", &mPerlin.mPerlinAmount, 0, 1);
   FLOATSLIDER(mWarbleSlider, "warble", &mPerlin.mPerlinWarble, 0, 1);
   FLOATSLIDER(mNoiseSlider, "noise", &mPerlin.mPerlinNoise, 0, 1);
   ENDUIBLOCK(mWidth, mHeight);

   mWarbleSlider->SetMode(FloatSlider::kSquare);
   mNoiseSlider->SetMode(FloatSlider::kSquare);
}

void UnstablePitch::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;

   ofPushStyle();
   ofRectangle rect(3, 3, mWidth - 6, 34);
   const int kGridSize = 30;
   ofFill();
   for (int col = 0; col < kGridSize; ++col)
   {
      float x = rect.x + col * (rect.width / kGridSize);
      float y = rect.y;
      float val = mPerlin.GetValue(gTime, x / rect.width * 10, 0) * ofClamp(mPerlin.mPerlinAmount * 5, 0, 1);
      ofSetColor(val * 255, 0, val * 255);
      ofRect(x, y, (rect.width / kGridSize) + .5f, rect.height + .5f, 0);
   }

   ofNoFill();
   ofSetColor(0, 255, 0, 90);
   for (int voice = 0; voice < kNumVoices; ++voice)
   {
      if (mIsVoiceUsed[voice])
      {
         ofBeginShape();
         for (int i = 0; i < gBufferSize; ++i)
         {
            float sample = ofClamp(mModulation.GetPitchBend(voice)->GetBufferValue(i) * 5, -1, 1);
            ofVertex((i * rect.width) / gBufferSize + rect.x, rect.y + (-sample * .5f + .5f) * rect.height);
         }
         ofEndShape();
      }
   }

   ofPopStyle();

   mAmountSlider->Draw();
   mWarbleSlider->Draw();
   mNoiseSlider->Draw();
}

void UnstablePitch::PlayNote(NoteMessage note)
{
   if (mEnabled)
   {
      if (note.voiceIdx == -1)
      {
         if (note.velocity > 0)
         {
            bool foundVoice = false;
            for (size_t i = 0; i < mIsVoiceUsed.size(); ++i)
            {
               int voiceToCheck = (i + mVoiceRoundRobin) % kNumVoices;
               if (mIsVoiceUsed[voiceToCheck] == false)
               {
                  note.voiceIdx = voiceToCheck;
                  mVoiceRoundRobin = (mVoiceRoundRobin + 1) % kNumVoices;
                  foundVoice = true;
                  break;
               }
            }

            if (!foundVoice)
            {
               note.voiceIdx = mVoiceRoundRobin;
               mVoiceRoundRobin = (mVoiceRoundRobin + 1) % kNumVoices;
            }
         }
         else
         {
            note.voiceIdx = mPitchToVoice[note.pitch];
         }
      }

      if (note.voiceIdx < 0 || note.voiceIdx >= kNumVoices)
         note.voiceIdx = 0;

      mIsVoiceUsed[note.voiceIdx] = note.velocity > 0;
      mPitchToVoice[note.pitch] = (note.velocity > 0) ? note.voiceIdx : -1;

      mModulation.GetPitchBend(note.voiceIdx)->AppendTo(note.modulation.pitchBend);
      note.modulation.pitchBend = mModulation.GetPitchBend(note.voiceIdx);
   }

   FillModulationBuffer(note.time, note.voiceIdx);
   PlayNoteOutput(note);
}

void UnstablePitch::OnTransportAdvanced(float amount)
{
   ComputeSliders(0);

   for (int voice = 0; voice < kNumVoices; ++voice)
   {
      if (mIsVoiceUsed[voice])
         FillModulationBuffer(gTime, voice);
   }
}

void UnstablePitch::FillModulationBuffer(double time, int voiceIdx)
{
   for (int i = 0; i < gBufferSize; ++i)
      gWorkBuffer[i] = ofMap(mPerlin.GetValue(time + i * gInvSampleRateMs, (time + i * gInvSampleRateMs) / 1000, voiceIdx), 0, 1, -mPerlin.mPerlinAmount, mPerlin.mPerlinAmount);

   mModulation.GetPitchBend(voiceIdx)->FillBuffer(gWorkBuffer);
}

void UnstablePitch::FloatSliderUpdated(FloatSlider* slider, float oldVal, double time)
{
}

void UnstablePitch::CheckboxUpdated(Checkbox* checkbox, double time)
{
   if (checkbox == mEnabledCheckbox)
   {
      if (!mEnabled)
      {
         for (size_t i = 0; i < mIsVoiceUsed.size(); ++i)
            mIsVoiceUsed[i] = false;
      }
   }
}

void UnstablePitch::LoadLayout(const ofxJSONElement& moduleInfo)
{
   mModuleSaveData.LoadString("target", moduleInfo);

   SetUpFromSaveData();
}

void UnstablePitch::SetUpFromSaveData()
{
   SetUpPatchCables(mModuleSaveData.GetString("target"));
}
