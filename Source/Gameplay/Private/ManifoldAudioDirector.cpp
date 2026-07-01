// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldAudioDirector.h"

int32 UManifoldAudioDirector::IntervalForRatio(int32 Num, int32 Den)
{
    if (Num <= 0 || Den <= 0)
    {
        return 0;
    }
    // Equal-tempered semitones for a just-intonation frequency ratio:
    //   semitones = 12 * log2(Num/Den), rounded to the nearest fret.
    const float Ratio = static_cast<float>(Num) / static_cast<float>(Den);
    return FMath::RoundToInt(12.0f * FMath::Log2(Ratio));
}

void UManifoldAudioDirector::AssignRealmVoice(FName Realm, int32& OutRootMidi, EManifoldAudioMode& OutMode) const
{
    // The four shipping realms get hand-tuned voices so their domains are instantly
    // distinguishable by ear: a vast low minor for celestial Orbits, a flowing
    // Dorian for Fluids, a pure Ionian for Harmonics, a shimmering Lydian for Waves.
    if (Realm == FName(TEXT("Orbits")))    { OutRootMidi = 48; OutMode = EManifoldAudioMode::Aeolian;    return; } // C3
    if (Realm == FName(TEXT("Fluids")))    { OutRootMidi = 55; OutMode = EManifoldAudioMode::Dorian;     return; } // G3
    if (Realm == FName(TEXT("Harmonics"))) { OutRootMidi = 60; OutMode = EManifoldAudioMode::Ionian;      return; } // C4
    if (Realm == FName(TEXT("Waves")))     { OutRootMidi = 62; OutMode = EManifoldAudioMode::Lydian;      return; } // D4

    // Any future realm still gets a stable, deterministic voice derived from its name,
    // so the audio identity is fixed the moment a realm is registered.
    const uint32 H = GetTypeHash(Realm);
    OutRootMidi = 48 + static_cast<int32>(H % 24u);          // spans two octaves, C3..B4
    OutMode = static_cast<EManifoldAudioMode>(H % 7u);       // one of the seven modes
}

FManifoldAudioCue UManifoldAudioDirector::CueForRealmAmbient(FName Realm) const
{
    FManifoldAudioCue Cue;
    Cue.Intent = EManifoldCueIntent::RealmAmbient;
    Cue.Realm = Realm;
    AssignRealmVoice(Realm, Cue.RootMidi, Cue.Mode);
    Cue.IntervalSemitones = 0;   // the tonic drone
    Cue.Intensity = 0.5f;        // ambient sits under everything
    return Cue;
}

FManifoldAudioCue UManifoldAudioDirector::CueForDiscovery(FName Realm, int32 RatioNum, int32 RatioDen) const
{
    FManifoldAudioCue Cue = CueForRealmAmbient(Realm);
    Cue.Intent = EManifoldCueIntent::DiscoveryChime;
    // The discovery literally sounds its ratio: a 3:2 rings as a perfect fifth.
    Cue.IntervalSemitones = IntervalForRatio(RatioNum, RatioDen);
    Cue.Intensity = 1.0f;
    return Cue;
}

FManifoldAudioCue UManifoldAudioDirector::CueForTransport(FName FromRealm, FName ToRealm) const
{
    FManifoldAudioCue Cue = CueForRealmAmbient(FromRealm);
    Cue.Intent = EManifoldCueIntent::ChordResolve;

    // Crossing the seam resolves toward the destination realm's tonic — the interval
    // is the melodic motion from the source root to the destination root.
    int32 ToRoot = 60;
    EManifoldAudioMode ToMode = EManifoldAudioMode::Ionian;
    AssignRealmVoice(ToRealm, ToRoot, ToMode);
    Cue.IntervalSemitones = ToRoot - Cue.RootMidi;
    Cue.Intensity = 1.0f;
    return Cue;
}
