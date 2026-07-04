// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "ManifoldAudioDirector.h"
#include "ManifoldToneSynth.h"
#include "HAL/IConsoleManager.h"

// The core of the audio layer: integer frequency ratios map to the correct
// consonant musical intervals. This is what makes a discovered correspondence
// *audible* — the 3:2 the physics finds is the perfect fifth you hear.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioIntervalForRatioTest, "MANIFOLD.Audio.IntervalForRatio", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAudioIntervalForRatioTest::RunTest(const FString& Parameters)
{
    UTEST_EQUAL("Unison 1:1 -> 0 semitones", UManifoldAudioDirector::IntervalForRatio(1, 1), 0);
    UTEST_EQUAL("Major third 5:4 -> 4 semitones", UManifoldAudioDirector::IntervalForRatio(5, 4), 4);
    UTEST_EQUAL("Perfect fourth 4:3 -> 5 semitones", UManifoldAudioDirector::IntervalForRatio(4, 3), 5);
    UTEST_EQUAL("Perfect fifth 3:2 -> 7 semitones", UManifoldAudioDirector::IntervalForRatio(3, 2), 7);
    UTEST_EQUAL("Octave 2:1 -> 12 semitones", UManifoldAudioDirector::IntervalForRatio(2, 1), 12);
    UTEST_EQUAL("Invalid ratio -> 0 semitones", UManifoldAudioDirector::IntervalForRatio(1, 0), 0);
    return true;
}

// Each realm has a stable, distinct musical identity (mode + tonic), so domains are
// told apart by ear. Deterministic: the same realm always sounds the same.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioRealmVoicesTest, "MANIFOLD.Audio.RealmVoices", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAudioRealmVoicesTest::RunTest(const FString& Parameters)
{
    UManifoldAudioDirector* Dir = NewObject<UManifoldAudioDirector>();

    const FManifoldAudioCue Orbits = Dir->CueForRealmAmbient(TEXT("Orbits"));
    const FManifoldAudioCue Harmonics = Dir->CueForRealmAmbient(TEXT("Harmonics"));

    UTEST_EQUAL("Ambient cue is a tonic drone (no interval)", Orbits.IntervalSemitones, 0);
    UTEST_EQUAL("Ambient intent is RealmAmbient", (int32)Orbits.Intent, (int32)EManifoldCueIntent::RealmAmbient);
    UTEST_TRUE("Orbits and Harmonics have distinct voices",
        Orbits.RootMidi != Harmonics.RootMidi || Orbits.Mode != Harmonics.Mode);

    // Deterministic identity.
    const FManifoldAudioCue OrbitsAgain = Dir->CueForRealmAmbient(TEXT("Orbits"));
    UTEST_EQUAL("Realm tonic is stable", OrbitsAgain.RootMidi, Orbits.RootMidi);
    UTEST_EQUAL("Realm mode is stable", (int32)OrbitsAgain.Mode, (int32)Orbits.Mode);

    return true;
}

// A discovery sounds its ratio over the realm's tonic; a transport resolves toward
// the destination realm's tonic. This is the discovery->resolve musical beat that
// the audio pipeline binds sounds to.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioDiscoveryAndResolveTest, "MANIFOLD.Audio.DiscoveryAndResolve", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAudioDiscoveryAndResolveTest::RunTest(const FString& Parameters)
{
    UManifoldAudioDirector* Dir = NewObject<UManifoldAudioDirector>();

    // Discovery of a 3:2 in Orbits -> a perfect fifth over the Orbits tonic.
    const FManifoldAudioCue Disc = Dir->CueForDiscovery(TEXT("Orbits"), 3, 2);
    UTEST_EQUAL("Discovery intent is DiscoveryChime", (int32)Disc.Intent, (int32)EManifoldCueIntent::DiscoveryChime);
    UTEST_EQUAL("Discovery voices the 3:2 as a perfect fifth", Disc.IntervalSemitones, 7);
    UTEST_EQUAL("Discovery keeps the realm's tonic", Disc.Realm, FName(TEXT("Orbits")));

    // Transport Fluids -> Orbits resolves toward the Orbits tonic.
    const FManifoldAudioCue From = Dir->CueForRealmAmbient(TEXT("Fluids"));
    const FManifoldAudioCue To = Dir->CueForRealmAmbient(TEXT("Orbits"));
    const FManifoldAudioCue Move = Dir->CueForTransport(TEXT("Fluids"), TEXT("Orbits"));
    UTEST_EQUAL("Transport intent is ChordResolve", (int32)Move.Intent, (int32)EManifoldCueIntent::ChordResolve);
    UTEST_EQUAL("Transport resolves from source toward destination tonic",
        Move.IntervalSemitones, To.RootMidi - From.RootMidi);

    return true;
}

// The audio is actually SYNTHESIZED (not just described): equal-tempered pitch and a
// decaying-sine voice that produces real audible energy and then falls silent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToneSynthesisTest, "MANIFOLD.Audio.ToneSynthesis", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FToneSynthesisTest::RunTest(const FString& Parameters)
{
    // Equal-tempered pitch: octaves double the frequency, A4 = 440 Hz.
    UTEST_EQUAL("MIDI 69 -> 440 Hz", FMath::RoundToInt(ManifoldMidiToFrequency(69)), 440);
    UTEST_EQUAL("MIDI 81 -> 880 Hz (octave up)", FMath::RoundToInt(ManifoldMidiToFrequency(81)), 880);
    UTEST_EQUAL("MIDI 57 -> 220 Hz (octave down)", FMath::RoundToInt(ManifoldMidiToFrequency(57)), 220);

    // A triggered voice produces real signal, then decays to silence.
    const int32 SampleRate = 48000;
    FManifoldToneVoice Voice;
    UTEST_FALSE("Voice starts silent", Voice.IsActive());
    Voice.NoteOn(440.0f, 1.0f);
    UTEST_TRUE("Voice is active after NoteOn", Voice.IsActive());

    double Energy = 0.0;
    float MaxAbs = 0.0f;
    for (int32 i = 0; i < SampleRate; ++i) // 1 second
    {
        const float S = Voice.NextSample(SampleRate);
        Energy += static_cast<double>(S) * S;
        MaxAbs = FMath::Max(MaxAbs, FMath::Abs(S));
    }
    UTEST_GREATER("Voice produced audible energy", Energy, 1.0);
    UTEST_GREATER("Voice reached a real amplitude", MaxAbs, 0.1f);

    for (int32 i = 0; i < SampleRate * 3; ++i) // decay for 3 more seconds
    {
        Voice.NextSample(SampleRate);
    }
    UTEST_FALSE("Voice decays to silence", Voice.IsActive());

    // A frequency above the sample rate must still yield finite, bounded samples
    // (the phase wrap must not let Phase grow without bound).
    FManifoldToneVoice Hi;
    Hi.NoteOn(60000.0f, 1.0f); // > 48 kHz sample rate
    bool bFinite = true;
    float MaxAbs2 = 0.0f;
    for (int32 i = 0; i < 20000; ++i)
    {
        const float S = Hi.NextSample(SampleRate);
        if (!FMath::IsFinite(S)) { bFinite = false; }
        MaxAbs2 = FMath::Max(MaxAbs2, FMath::Abs(S));
    }
    UTEST_TRUE("High-frequency voice stays finite", bFinite);
    UTEST_TRUE("High-frequency voice stays bounded", MaxAbs2 <= 1.01f);

    return true;
}

// The four shipping realms get hand-tuned voices; every OTHER realm (Rhythm, Gears, Circuits — all
// voiced live by the GameMode's ambient pad) falls to a deterministic hash voice. Existing tests
// only cover the hand-tuned realms, so this locks the fallback contract: a valid, in-range, stable
// ambient voice that genuinely varies by realm — so a change to the 48/24/7 constants or the hash
// can't silently reassign every unnamed realm's sound with nothing failing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioHashFallbackVoicesTest, "MANIFOLD.Audio.HashFallbackVoices", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAudioHashFallbackVoicesTest::RunTest(const FString& Parameters)
{
    UManifoldAudioDirector* Dir = NewObject<UManifoldAudioDirector>();

    // Circuits is not hand-tuned, so it exercises the hash fallback.
    const FManifoldAudioCue Circuits = Dir->CueForRealmAmbient(TEXT("Circuits"));
    UTEST_TRUE("fallback root sits in the intended C3..B4 span [48,71]",
        Circuits.RootMidi >= 48 && Circuits.RootMidi <= 71);
    UTEST_TRUE("fallback mode is one of the seven church modes [0,6]",
        (int32)Circuits.Mode >= 0 && (int32)Circuits.Mode <= 6);
    UTEST_EQUAL("fallback ambient is a tonic drone (no interval)", Circuits.IntervalSemitones, 0);
    UTEST_EQUAL("fallback ambient intent is RealmAmbient",
        (int32)Circuits.Intent, (int32)EManifoldCueIntent::RealmAmbient);
    UTEST_EQUAL("fallback ambient sits at the ambient intensity", Circuits.Intensity, 0.5f);

    // Deterministic within a run: the same realm always sounds the same.
    const FManifoldAudioCue CircuitsAgain = Dir->CueForRealmAmbient(TEXT("Circuits"));
    UTEST_EQUAL("fallback root is stable", CircuitsAgain.RootMidi, Circuits.RootMidi);
    UTEST_EQUAL("fallback mode is stable", (int32)CircuitsAgain.Mode, (int32)Circuits.Mode);

    // The hash actually differentiates realms: three unnamed realms don't all collapse to one voice
    // (which is exactly what a broken or ignored hash would produce).
    const FManifoldAudioCue Rhythm = Dir->CueForRealmAmbient(TEXT("Rhythm"));
    const FManifoldAudioCue Gears  = Dir->CueForRealmAmbient(TEXT("Gears"));
    const bool bAllSame =
        Rhythm.RootMidi == Gears.RootMidi && Gears.RootMidi == Circuits.RootMidi &&
        Rhythm.Mode == Gears.Mode && Gears.Mode == Circuits.Mode;
    UTEST_FALSE("the hash fallback gives unnamed realms distinct voices (not all identical)", bAllSame);

    return true;
}

// The procedural synth must be MUTED BY DEFAULT: development, CI, and headless renders should make
// no sound (the game only starts real-time audio when manifold.MuteAudio==0, which production sets).
// This locks the ship-safe default so a regression can't silently un-mute every dev run. The
// AManifoldGameMode translation unit registers the CVar when the Gameplay module loads.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioMutedByDefaultTest, "MANIFOLD.Audio.MutedByDefault", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAudioMutedByDefaultTest::RunTest(const FString& Parameters)
{
    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("manifold.MuteAudio"));
    UTEST_TRUE("manifold.MuteAudio CVar is registered", CVar != nullptr);
    if (CVar)
    {
        UTEST_EQUAL("audio is muted by default (silent until production)", CVar->GetInt(), 1);
    }
    return true;
}
