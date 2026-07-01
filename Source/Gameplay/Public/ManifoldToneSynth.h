// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "ManifoldAudioDirector.h"
#include "ManifoldToneSynth.generated.h"

/** MIDI note number -> frequency in Hz (A4 = MIDI 69 = 440 Hz). */
MANIFOLDGAMEPLAY_API float ManifoldMidiToFrequency(int32 Midi);

/**
 * A single synthesizer voice: a sine oscillator with an exponential decay (a soft
 * "chime"/"bell" envelope). Pure DSP with no engine dependencies, so it is
 * deterministic and unit-testable headlessly.
 */
struct MANIFOLDGAMEPLAY_API FManifoldToneVoice
{
    float Frequency = 440.0f;
    float Amplitude = 0.0f;      // current (post-decay) amplitude
    double Phase = 0.0;          // radians
    float DecayPerSecond = 4.0f; // exponential decay rate

    bool IsActive() const { return Amplitude > 1.0e-4f; }

    /** Trigger the voice at a frequency and starting amplitude. */
    void NoteOn(float InFrequency, float InAmplitude)
    {
        Frequency = InFrequency;
        Amplitude = InAmplitude;
        Phase = 0.0;
    }

    /** Produce the next sample and advance phase + decay. Returns 0 when silent. */
    float NextSample(int32 SampleRate)
    {
        if (SampleRate <= 0 || !IsActive())
        {
            return 0.0f;
        }
        const float Sample = Amplitude * FMath::Sin(static_cast<float>(Phase));
        Phase += 2.0 * PI * static_cast<double>(Frequency) / static_cast<double>(SampleRate);
        if (Phase >= 2.0 * PI)
        {
            Phase -= 2.0 * PI;
        }
        Amplitude -= Amplitude * (DecayPerSecond / static_cast<float>(SampleRate));
        return Sample;
    }
};

/**
 * UManifoldSynthComponent — actually PLAYS the audio the AudioDirector describes.
 *
 * It converts a FManifoldAudioCue (root MIDI + interval + intensity) into a tone and
 * mixes a small pool of decaying-sine voices in real time on the audio thread. No
 * external sound asset is required: the sound is synthesized in code. This is what
 * makes "the correspondence you can hear" literally audible.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MANIFOLDGAMEPLAY_API UManifoldSynthComponent : public USynthComponent
{
    GENERATED_BODY()

public:
    /** Trigger a tone for a cue (safe to call from the game thread). */
    void PlayCue(const FManifoldAudioCue& Cue);

protected:
    virtual bool Init(int32& SampleRate) override;
    virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
    static constexpr int32 MaxVoices = 16;
    FManifoldToneVoice Voices[MaxVoices];
    int32 NextVoice = 0;
    int32 CachedSampleRate = 48000;
    FCriticalSection VoiceLock; // OnGenerateAudio runs on the audio render thread
};
