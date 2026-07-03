// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldToneSynth.h"

float ManifoldMidiToFrequency(int32 Midi)
{
    // Equal temperament, A4 (MIDI 69) = 440 Hz.
    return 440.0f * FMath::Pow(2.0f, static_cast<float>(Midi - 69) / 12.0f);
}

bool UManifoldSynthComponent::Init(int32& SampleRate)
{
    NumChannels = 1; // mono
    CachedSampleRate.store(SampleRate, std::memory_order_relaxed);
    return true;
}

void UManifoldSynthComponent::PlayCue(const FManifoldAudioCue& Cue)
{
    const int32 Midi = Cue.RootMidi + Cue.IntervalSemitones;
    // Clamp below Nyquist: anything above is inaudible aliasing regardless, and it
    // keeps the per-sample phase increment sane for any (even crafted) cue.
    const float Frequency = FMath::Clamp(ManifoldMidiToFrequency(Midi), 0.0f, 0.45f * CachedSampleRate.load(std::memory_order_relaxed));
    // Keep the mix well below clipping even with several overlapping voices.
    const float Amplitude = FMath::Clamp(Cue.Intensity, 0.0f, 1.0f) * 0.2f;

    FScopeLock Lock(&VoiceLock);
    Voices[NextVoice].NoteOn(Frequency, Amplitude);
    NextVoice = (NextVoice + 1) % MaxVoices;
}

int32 UManifoldSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
    FScopeLock Lock(&VoiceLock);
    const int32 SampleRate = CachedSampleRate.load(std::memory_order_relaxed);
    for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
    {
        float Mixed = 0.0f;
        for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
        {
            Mixed += Voices[VoiceIndex].NextSample(SampleRate);
        }
        OutAudio[SampleIndex] = FMath::Clamp(Mixed, -1.0f, 1.0f);
    }
    return NumSamples;
}
