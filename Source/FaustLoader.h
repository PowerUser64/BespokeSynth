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
    FaustLoader.h
    Created: 2025-02-08
    Author:  Blake North
  ==============================================================================
*/

#pragma once
#include "IAudioProcessor.h"
#include "IDrawableModule.h"

class FaustLoader : public IAudioProcessor, public IDrawableModule
{
public:
   FaustLoader()
   : IAudioProcessor(512)
   {}

   static IDrawableModule* Create() { return new FaustLoader(); }
   static bool CanCreate() { return true; }

protected:
   void SyncBuffers(int overrideNumOutputChannels = -1);

   PatchCableSource* GetPatchCableSource(int index = 0) override { return nullptr; }
   void DrawModule() override;
   void Process(double time) override;
};