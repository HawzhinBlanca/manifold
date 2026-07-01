// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ManifoldAudioDirector.generated.h"

/**
 * The seven diatonic church modes. Each realm is assigned one so the domains are
 * sonically distinct — the audio counterpart of giving each realm its own palette.
 */
UENUM(BlueprintType)
enum class EManifoldAudioMode : uint8
{
    Ionian,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Aeolian,
    Locrian
};

/**
 * What a cue is FOR. The escalation mirrors the gameplay beat:
 *   RealmAmbient  — the steady drone of a realm you are inside
 *   DiscoveryChime — you surfaced a structure; its ratio rings out as an interval
 *   ChordResolve  — you transported it across the seam; the tension resolves
 */
UENUM(BlueprintType)
enum class EManifoldCueIntent : uint8
{
    RealmAmbient,
    DiscoveryChime,
    ChordResolve
};

/**
 * A fully-resolved audio intent. This is *data*: it says what should be played and
 * in what musical terms, without owning any sound asset. The art/audio pipeline
 * binds a USoundBase / MetaSound to each (Intent, Mode) pair later; the game logic
 * only ever produces these cues, so the routing is testable headlessly.
 */
USTRUCT(BlueprintType)
struct MANIFOLDGAMEPLAY_API FManifoldAudioCue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD|Audio")
    EManifoldCueIntent Intent = EManifoldCueIntent::RealmAmbient;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD|Audio")
    FName Realm;

    /** MIDI note number of the realm's tonic (60 = middle C). */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD|Audio")
    int32 RootMidi = 60;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD|Audio")
    EManifoldAudioMode Mode = EManifoldAudioMode::Ionian;

    /** The interval (in semitones above the root) that voices this cue. */
    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD|Audio")
    int32 IntervalSemitones = 0;

    UPROPERTY(BlueprintReadOnly, Category = "MANIFOLD|Audio")
    float Intensity = 1.0f;
};

/**
 * UManifoldAudioDirector — turns simulation events into musical intent.
 *
 * The heart of it is a genuine mapping from the integer frequency ratios the game
 * is built on to consonant musical intervals: a 3:2 (the resonance every realm
 * shares) becomes a perfect fifth, a 2:1 an octave. So when a cross-domain analogy
 * is discovered, the *sound* of it is literally the same interval the physics found
 * — the correspondence you can hear.
 *
 * This layer is deterministic and asset-free, so it is unit-tested headlessly; the
 * audio pipeline only has to attach sounds to the cues it emits.
 */
UCLASS(BlueprintType)
class MANIFOLDGAMEPLAY_API UManifoldAudioDirector : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Frequency ratio Num:Den -> nearest equal-tempered interval in semitones.
     * 3:2 -> 7 (perfect fifth), 2:1 -> 12 (octave), 4:3 -> 5 (perfect fourth),
     * 5:4 -> 4 (major third), 1:1 -> 0 (unison). Invalid input -> 0.
     */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Audio")
    static int32 IntervalForRatio(int32 Num, int32 Den);

    /** The steady voice of a realm: its tonic drone. Stable per realm name. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Audio")
    FManifoldAudioCue CueForRealmAmbient(FName Realm) const;

    /** A discovery in a realm, sounded as its ratio over the realm's tonic. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Audio")
    FManifoldAudioCue CueForDiscovery(FName Realm, int32 RatioNum, int32 RatioDen) const;

    /** Transporting across the seam: resolve toward the destination realm's tonic. */
    UFUNCTION(BlueprintPure, Category = "MANIFOLD|Audio")
    FManifoldAudioCue CueForTransport(FName FromRealm, FName ToRealm) const;

private:
    /** Deterministic realm identity: hand-tuned for the named realms, hashed otherwise. */
    void AssignRealmVoice(FName Realm, int32& OutRootMidi, EManifoldAudioMode& OutMode) const;
};
