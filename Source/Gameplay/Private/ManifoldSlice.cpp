// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
#include "RhythmKernel.h"
#include "GearsKernel.h"
#include "CorrespondenceSystem.h"
#include "TelemetrySystem.h"
#include "DeterministicRNG.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

void UManifoldSlice::Setup(uint64 OrbitsSeed, uint64 FluidsSeed)
{
    SavedOrbitsSeed = OrbitsSeed;
    SavedFluidsSeed = FluidsSeed;

    // Reset all session accumulators so reusing a slice instance (e.g. RecordReplay
    // then RunReplay on the same object) starts clean instead of accumulating.
    IgnitedCount = 0;
    TransportCount = 0;
    SharedDiscoveries = 0;
    CurrentStep = 0;
    CurrentTime = 0.0f;
    bCorrespondenceAvailable = false;
    PendingVortexId = FGuid();
    SessionState = EManifoldSessionState::InProgress;
    TransportStepLog.Reset();
    AudioCues.Reset();
    LastAudioCue = FManifoldAudioCue();

    // Not a constellation session (SetupConstellation sets this true).
    bConstellationMode = false;
    ConstellationSize = 0;
    ActiveRelation = ECorrespondenceRelation::Exact;
    Constellation.Reset();
    ConstellationRealmIds.Reset();
    RealmSurfaceRatios.Reset();
    FailedProbes = 0;

    Orbits = NewObject<UOrbitsKernel>(this);
    Fluids = NewObject<UFluidsKernel>(this);
    Harmonics = NewObject<UHarmonicsKernel>(this);
    Waves = NewObject<UWavesKernel>(this);
    Rhythm = NewObject<URhythmKernel>(this);
    Gears = NewObject<UGearsKernel>(this);
    Decoy = NewObject<UHarmonicsKernel>(this);
    Correspond = NewObject<UCorrespondenceSystem>(this);
    Telemetry = NewObject<UTelemetrySystem>(this);
    Audio = NewObject<UManifoldAudioDirector>(this);

    Orbits->Initialize(OrbitsSeed);
    Fluids->Initialize(FluidsSeed);
    Harmonics->Initialize(OrbitsSeed ^ 0xABCDEF);
    Waves->Initialize(OrbitsSeed ^ 0x123456);
    Rhythm->Initialize(OrbitsSeed ^ 0x789ABC);
    Gears->Initialize(OrbitsSeed ^ 0x2468AC);
    Decoy->Initialize(OrbitsSeed ^ 0xFEDCBA);
    Correspond->RegisterKernels(Orbits, Fluids);

    // Generic N-realm engine: any realms exposing the same ratio correspond. The
    // hidden ratio shows up across FOUR ratio domains — celestial, acoustic, spatial,
    // temporal — plus a DECOY realm whose ratio deliberately differs, so the engine
    // must (and does) refuse the false correspondence.
    Correspond->RegisterRealm(TEXT("Orbits"), TEXT("OrbitalResonance"), Orbits);
    Correspond->RegisterRealm(TEXT("Harmonics"), TEXT("HarmonicRatio"), Harmonics);
    Correspond->RegisterRealm(TEXT("Waves"), TEXT("WaveHarmonic"), Waves);
    Correspond->RegisterRealm(TEXT("Rhythm"), TEXT("RhythmRatio"), Rhythm);
    Correspond->RegisterRealm(TEXT("Gears"), TEXT("GearRatio"), Gears);
    Correspond->RegisterRealm(TEXT("Decoy"), TEXT("HarmonicRatio"), Decoy);

    Telemetry->InitializeTelemetry(TEXT("SlicePlaythrough.log"));

    // The PUZZLE: this session's realms all secretly share ONE hidden ratio, chosen
    // deterministically from the seed. Different seeds hide different ratios (3:2, 4:3,
    // 5:3, 2:1, ...), so no two sessions are the same and the game can't be pre-solved
    // — the design's "un-pre-computable" pillar, made real.
    PickSharedRatio(OrbitsSeed, SharedP, SharedQ);
    const double RatioD = static_cast<double>(SharedP) / static_cast<double>(SharedQ);

    // The decoy gets a DIFFERENT ratio: advance the seed until it picks another one.
    {
        uint64 DecoySeed = OrbitsSeed + 1;
        do { PickSharedRatio(DecoySeed++, DecoyP, DecoyQ); }
        while (DecoyP == SharedP && DecoyQ == SharedQ);
    }

    // Correspondence content, generated as data from the chosen ratio (data-driven:
    // the CorrespondenceSystem matches Orbits<->Fluids against this spec + tolerance).
    UCorrespondenceMapping* Mapping = NewObject<UCorrespondenceMapping>(this);
    FCorrespondenceSpec Spec;
    Spec.SourceStructureType = TEXT("OrbitalResonance");
    Spec.TargetStructureType = TEXT("VortexCenter");
    Spec.MatchingRatio = FString::Printf(TEXT("%d:%d"), SharedP, SharedQ);
    Spec.Tolerance = 0.05f;
    Spec.ScaleFactor = 1.0f;
    Mapping->Specs.Add(Spec);
    Correspond->InitializeMapping(Mapping);

    // Gameplay reactions: on a detected correspondence, transport power across the
    // seam; record every discovery/transport for the Insight Rate.
    Correspond->OnCorrespondenceIgnited.AddUObject(this, &UManifoldSlice::HandleIgnited);
    Correspond->OnSharedStructureDiscovered.AddUObject(this, &UManifoldSlice::HandleSharedDiscovery);
    Correspond->OnTransportCompleted.AddUObject(this, &UManifoldSlice::HandleTransport);

    // --- Orbits scenario: two planets in a P:Q mean-motion resonance ---
    FOrbitsBodyDef Star;
    Star.Name = TEXT("Star");
    Star.Mass = 1.989e30;      // 1 solar mass
    Star.bIsCentral = true;
    Orbits->AddBody(Star);

    FOrbitsBodyDef PlanetA;
    PlanetA.Name = TEXT("PlanetA");
    PlanetA.Mass = 1e24;
    PlanetA.Position = FVector(1.496e11, 0.0, 0.0); // 1 AU
    const double V_A = FMath::Sqrt((Orbits->G * Star.Mass) / PlanetA.Position.X);
    PlanetA.Velocity = FVector(0.0, V_A, 0.0);
    Orbits->AddBody(PlanetA);

    // Semi-major axis for a period ratio of P:Q (Kepler's third law).
    const double R_B = PlanetA.Position.X * FMath::Pow(RatioD, 2.0 / 3.0);
    FOrbitsBodyDef PlanetB;
    PlanetB.Name = TEXT("PlanetB");
    PlanetB.Mass = 1e24;
    PlanetB.Position = FVector(R_B, 0.0, 0.0);
    const double V_B = FMath::Sqrt((Orbits->G * Star.Mass) / R_B);
    PlanetB.Velocity = FVector(0.0, V_B, 0.0);
    Orbits->AddBody(PlanetB);

    // --- Fluids scenario: a counter-clockwise circulation around the grid centre ---
    Fluids->AddVelocity(0.5f, 0.45f, 20.0f, 0.0f);
    Fluids->AddVelocity(0.55f, 0.5f, 0.0f, 20.0f);
    Fluids->AddVelocity(0.5f, 0.55f, -20.0f, 0.0f);
    Fluids->AddVelocity(0.45f, 0.5f, 0.0f, -20.0f);

    // --- Harmonics: modes at Q Hz and P Hz form the same P:Q ratio ---
    Harmonics->AddMode(static_cast<double>(SharedQ), 1.0);
    Harmonics->AddMode(static_cast<double>(SharedP), 1.0);

    // --- Waves: the Q-th and P-th string harmonics form the same P:Q ratio ---
    Waves->ExciteHarmonic(SharedQ, 1.0);
    Waves->ExciteHarmonic(SharedP, 1.0);

    // --- Rhythm: a P-against-Q polyrhythm — the same structure in the domain of TIME ---
    Rhythm->AddVoice(static_cast<double>(SharedP));
    Rhythm->AddVoice(static_cast<double>(SharedQ));

    // --- Gears: two meshed gears with P and Q teeth — the same P:Q, exactly, in the
    //     domain of MECHANISM ---
    Gears->AddGear(SharedP);
    Gears->AddGear(SharedQ);

    // --- Decoy: a harmonic ratio that deliberately does NOT match the hidden one.
    //     The correspondence engine must refuse to pair it with the true realms. ---
    Decoy->AddMode(static_cast<double>(DecoyQ), 1.0);
    Decoy->AddMode(static_cast<double>(DecoyP), 1.0);
}

FManifoldExpeditionResult UManifoldSlice::RunExpedition(int64 BaseSeed, int32 NumLevels, int32 StepsPerLevel, UObject* Outer)
{
    FManifoldExpeditionResult Result;
    UObject* Package = Outer ? Outer : (UObject*)GetTransientPackage();

    for (int32 Level = 0; Level < NumLevels; ++Level)
    {
        UManifoldSlice* Slice = NewObject<UManifoldSlice>(Package);

        // Escalating difficulty: each level demands more discoveries. A single session
        // can surface at most 11 (1 seam + C(5,2)=10 cross-domain analogies), so the
        // target eventually exceeds what's discoverable and the expedition ends.
        FManifoldObjective Obj;
        Obj.TargetDiscoveries = 2 + Level * 2;
        Obj.StepBudget = 0;
        Slice->SetObjective(Obj);

        Slice->Setup(static_cast<uint64>(BaseSeed + Level), static_cast<uint64>(BaseSeed * 7 + Level));
        Slice->RunPlaythrough(StepsPerLevel);

        const FManifoldSessionSummary Summary = Slice->GetSessionSummary();
        if (Summary.State != EManifoldSessionState::Won)
        {
            break; // the expedition ends at the first level you can't clear
        }
        ++Result.LevelsCleared;
        Result.TotalScore += Summary.Score;
    }

    Result.bCompleted = (Result.LevelsCleared == NumLevels);
    return Result;
}

void UManifoldSlice::PickSharedRatio(uint64 Seed, int32& OutP, int32& OutQ)
{
    // Curated coprime small-integer ratios (p > q, both <= 9 so every realm can
    // realize and detect them). Each is a distinct, recognizable interval/resonance.
    static const FIntPoint Ratios[] = {
        FIntPoint(3, 2), FIntPoint(4, 3), FIntPoint(5, 4), FIntPoint(5, 3),
        FIntPoint(2, 1), FIntPoint(5, 2), FIntPoint(7, 4), FIntPoint(7, 5),
        FIntPoint(8, 5), FIntPoint(9, 8),
    };
    const int32 Count = UE_ARRAY_COUNT(Ratios);
    const int32 Index = static_cast<int32>(Seed % static_cast<uint64>(Count));
    OutP = Ratios[Index].X;
    OutQ = Ratios[Index].Y;
}

// ===========================================================================
// CONSTELLATION LOCK — deterministic scenario generation + the player's lock verb.
// ===========================================================================

namespace
{
    // A reduced small-integer ratio p:q (p > q >= 1, coprime), realizable by ALL six
    // ratio realms — Orbits (Kepler, MaxRatio=10), Harmonics, Waves, Rhythm, Gears, Decoy.
    struct FRatioPQ { int32 P; int32 Q; };

    // Octave families: each row's members all normalize to the SAME octave class (base:1)
    // yet are SURFACE-DISTINCT, and every term is <= 9 so Orbits detects them exactly.
    //   base 3 -> {3:1, 3:2, 6:1}   base 5 -> {5:1, 5:2, 5:4}
    //   base 7 -> {7:1, 7:2, 7:4}   base 9 -> {9:1, 9:2, 9:4}
    static const int32 kFamilyCount = 4;
    static const FRatioPQ kFamilyMembers[kFamilyCount][3] = {
        { {3,1}, {3,2}, {6,1} },
        { {5,1}, {5,2}, {5,4} },
        { {7,1}, {7,2}, {7,4} },
        { {9,1}, {9,2}, {9,4} },
    };

    // Decoy ratios for octave mode. Their octave classes are 5:3, 1:3, 7:5, 1:5, 7:3, 9:5
    // respectively — all MUTUALLY DISTINCT and none of the base:1 form a family uses, so a
    // decoy never corresponds to a member or to another decoy under OctaveInvariant.
    static const FRatioPQ kDecoyRatios[] = {
        {5,3}, {4,3}, {7,5}, {8,5}, {7,3}, {9,5},
    };

    // Fisher-Yates shuffle in place with the deterministic RNG.
    template <typename T>
    void DetShuffle(TArray<T>& A, FDeterministicRNG& RNG)
    {
        for (int32 i = A.Num() - 1; i > 0; --i)
        {
            const int32 j = RNG.RandRange(0, i);
            A.Swap(i, j);
        }
    }
}

ECorrespondenceRelation UManifoldSlice::PickRelation(uint64 Seed)
{
    // Two slice-realizable relations (Reciprocal needs q:p ordering that the order-
    // insensitive kernels can't emit, so it is engine-only). 50/50 from a mixed seed.
    FDeterministicRNG RNG(Seed ^ 0xABCDEF0123456789ULL);
    return RNG.RandBool() ? ECorrespondenceRelation::OctaveInvariant : ECorrespondenceRelation::Exact;
}

void UManifoldSlice::PickConstellation(uint64 Seed, int32 NumRealms, int32 K, TArray<int32>& OutMembers)
{
    OutMembers.Reset();
    NumRealms = FMath::Max(0, NumRealms);
    K = FMath::Clamp(K, 0, NumRealms);

    TArray<int32> Pool;
    Pool.Reserve(NumRealms);
    for (int32 i = 0; i < NumRealms; ++i) { Pool.Add(i); }

    // Partial Fisher-Yates: select K distinct indices deterministically.
    FDeterministicRNG RNG(Seed ^ 0x5EED5A1AD5C0FFEEULL);
    for (int32 i = 0; i < K; ++i)
    {
        const int32 j = i + RNG.RandRange(0, NumRealms - 1 - i);
        Pool.Swap(i, j);
    }
    Pool.SetNum(K);
    Pool.Sort();
    OutMembers = MoveTemp(Pool);
}

void UManifoldSlice::SetupConstellation(int64 InSeed, int32 InConstellationSize)
{
    const uint64 Seed = static_cast<uint64>(InSeed);
    SavedOrbitsSeed = Seed;
    SavedFluidsSeed = Seed;

    // Reset all session accumulators (reuse safety).
    IgnitedCount = 0;
    TransportCount = 0;
    SharedDiscoveries = 0;
    CurrentStep = 0;
    CurrentTime = 0.0f;
    bCorrespondenceAvailable = false;
    PendingVortexId = FGuid();
    SessionState = EManifoldSessionState::InProgress;
    TransportStepLog.Reset();
    AudioCues.Reset();
    LastAudioCue = FManifoldAudioCue();
    FailedProbes = 0;

    const int32 N = 6; // Orbits, Harmonics, Waves, Rhythm, Gears, Decoy
    ConstellationSize = FMath::Clamp(InConstellationSize, 2, 3);
    bConstellationMode = true;

    ActiveRelation = PickRelation(Seed);
    PickConstellation(Seed, N, ConstellationSize, Constellation);
    const TSet<int32> MemberSet(Constellation);

    // --- Choose surface ratios: members correspond under ActiveRelation, decoys don't. ---
    TArray<FRatioPQ> RealmRatios;
    RealmRatios.SetNum(N);

    FDeterministicRNG RNG(Seed ^ 0xC0FFEE1234567890ULL);

    if (ActiveRelation == ECorrespondenceRelation::OctaveInvariant)
    {
        // Members: three surface-distinct ratios from one octave family.
        const int32 Family = RNG.RandRange(0, kFamilyCount - 1);
        TArray<FRatioPQ> Members;
        for (int32 m = 0; m < 3; ++m) { Members.Add(kFamilyMembers[Family][m]); }
        DetShuffle(Members, RNG);

        // Decoys: distinct-class ratios, none in this family's class.
        const int32 FamilyBase = kFamilyMembers[Family][0].P; // 3/5/7/9
        TArray<FRatioPQ> Decoys;
        for (const FRatioPQ& R : kDecoyRatios)
        {
            // Reject any decoy whose octave class collides with the members' base:1.
            const FString DecoyClass = UCorrespondenceSystem::NormalizeRatio(
                FString::Printf(TEXT("%d:%d"), R.P, R.Q), ECorrespondenceRelation::OctaveInvariant);
            const FString MemberClass = FString::Printf(TEXT("%d:1"), FamilyBase);
            if (DecoyClass != MemberClass) { Decoys.Add(R); }
        }
        DetShuffle(Decoys, RNG);

        int32 mi = 0, di = 0;
        for (int32 idx = 0; idx < N; ++idx)
        {
            RealmRatios[idx] = MemberSet.Contains(idx) ? Members[mi++] : Decoys[di++];
        }
    }
    else // Exact: members share one literal ratio; decoys are distinct literals.
    {
        int32 rp = 0, rq = 0;
        PickSharedRatio(Seed, rp, rq);
        const FRatioPQ Shared{ rp, rq };

        TArray<FRatioPQ> Decoys;
        static const FIntPoint ExactPool[] = {
            FIntPoint(3,2), FIntPoint(4,3), FIntPoint(5,4), FIntPoint(5,3),
            FIntPoint(2,1), FIntPoint(5,2), FIntPoint(7,4), FIntPoint(7,5),
            FIntPoint(8,5), FIntPoint(9,8),
        };
        for (const FIntPoint& R : ExactPool)
        {
            if (R.X != Shared.P || R.Y != Shared.Q) { Decoys.Add(FRatioPQ{ R.X, R.Y }); }
        }
        DetShuffle(Decoys, RNG);

        int32 di = 0;
        for (int32 idx = 0; idx < N; ++idx)
        {
            RealmRatios[idx] = MemberSet.Contains(idx) ? Shared : Decoys[di++];
        }
    }

    // --- Build the realms and realize each realm's assigned ratio. ---
    Orbits = NewObject<UOrbitsKernel>(this);
    Harmonics = NewObject<UHarmonicsKernel>(this);
    Waves = NewObject<UWavesKernel>(this);
    Rhythm = NewObject<URhythmKernel>(this);
    Gears = NewObject<UGearsKernel>(this);
    Decoy = NewObject<UHarmonicsKernel>(this);
    Fluids = nullptr; // no Orbits<->Fluids seam in constellation mode
    Correspond = NewObject<UCorrespondenceSystem>(this);
    Telemetry = NewObject<UTelemetrySystem>(this);
    Audio = NewObject<UManifoldAudioDirector>(this);

    Orbits->Initialize(Seed);
    Harmonics->Initialize(Seed ^ 0xABCDEF);
    Waves->Initialize(Seed ^ 0x123456);
    Rhythm->Initialize(Seed ^ 0x789ABC);
    Gears->Initialize(Seed ^ 0x2468AC);
    Decoy->Initialize(Seed ^ 0xFEDCBA);

    Telemetry->InitializeTelemetry(TEXT("ConstellationSession.log"));
    Correspond->SetActiveRelation(ActiveRelation);
    Correspond->OnSharedStructureDiscovered.AddUObject(this, &UManifoldSlice::HandleSharedDiscovery);

    // Orbits realizes its ratio via a Kepler period ratio p:q (Star + two planets).
    auto SetupOrbitsRatio = [this](int32 P, int32 Q)
    {
        FOrbitsBodyDef Star; Star.Name = TEXT("Star"); Star.Mass = 1.989e30; Star.bIsCentral = true;
        Orbits->AddBody(Star);

        FOrbitsBodyDef A; A.Name = TEXT("PlanetA"); A.Mass = 1e24; A.Position = FVector(1.496e11, 0.0, 0.0);
        A.Velocity = FVector(0.0, FMath::Sqrt((Orbits->G * Star.Mass) / A.Position.X), 0.0);
        Orbits->AddBody(A);

        const double RatioD = static_cast<double>(P) / static_cast<double>(Q);
        const double R_B = A.Position.X * FMath::Pow(RatioD, 2.0 / 3.0);
        FOrbitsBodyDef B; B.Name = TEXT("PlanetB"); B.Mass = 1e24; B.Position = FVector(R_B, 0.0, 0.0);
        B.Velocity = FVector(0.0, FMath::Sqrt((Orbits->G * Star.Mass) / R_B), 0.0);
        Orbits->AddBody(B);
    };

    SetupOrbitsRatio(RealmRatios[0].P, RealmRatios[0].Q);
    Harmonics->AddMode(static_cast<double>(RealmRatios[1].Q), 1.0);
    Harmonics->AddMode(static_cast<double>(RealmRatios[1].P), 1.0);
    Waves->ExciteHarmonic(RealmRatios[2].Q, 1.0);
    Waves->ExciteHarmonic(RealmRatios[2].P, 1.0);
    Rhythm->AddVoice(static_cast<double>(RealmRatios[3].P));
    Rhythm->AddVoice(static_cast<double>(RealmRatios[3].Q));
    Gears->AddGear(RealmRatios[4].P);
    Gears->AddGear(RealmRatios[4].Q);
    Decoy->AddMode(static_cast<double>(RealmRatios[5].Q), 1.0);
    Decoy->AddMode(static_cast<double>(RealmRatios[5].P), 1.0);

    // Step each realm a few times so its structure detector populates ActiveRatios.
    for (int32 s = 0; s < 3; ++s)
    {
        Orbits->Step(0.1f);
        Harmonics->Step(0.016f);
        Waves->Step(0.001f);
        Rhythm->Step(0.016f);
        Gears->Step(0.016f);
        Decoy->Step(0.016f);
    }

    // Register the six realms under their structure query types (display order = index).
    // Realm 5 is a second oscillator bank ("Chords"); in constellation mode it can be a
    // true member, so it is NOT labelled "Decoy" (which it is only in the classic mode).
    ConstellationRealmIds = { TEXT("Orbits"), TEXT("Harmonics"), TEXT("Waves"),
                              TEXT("Rhythm"), TEXT("Gears"), TEXT("Chords") };
    static const FName QueryTypes[N] = { TEXT("OrbitalResonance"), TEXT("HarmonicRatio"),
        TEXT("WaveHarmonic"), TEXT("RhythmRatio"), TEXT("GearRatio"), TEXT("HarmonicRatio") };
    UObject* const Kernels[N] = { Orbits, Harmonics, Waves, Rhythm, Gears, Decoy };
    for (int32 idx = 0; idx < N; ++idx)
    {
        Correspond->RegisterRealm(ConstellationRealmIds[idx], QueryTypes[idx], Kernels[idx]);
    }

    // Record the surface ratio each realm actually reports (what the player sees).
    RealmSurfaceRatios.SetNum(N);
    for (int32 idx = 0; idx < N; ++idx)
    {
        RealmSurfaceRatios[idx] = FString::Printf(TEXT("%d:%d"), RealmRatios[idx].P, RealmRatios[idx].Q);
    }

    // Win when the player surfaces every corresponding pair in the constellation.
    FManifoldObjective Obj;
    Obj.TargetDiscoveries = FMath::Max(1, ConstellationSize * (ConstellationSize - 1) / 2);
    Obj.StepBudget = 0;
    Objective = Obj;
}

bool UManifoldSlice::PlayerLockConstellation(const TArray<int32>& SelectedRealmIndices)
{
    if (!bConstellationMode || SessionState != EManifoldSessionState::InProgress)
    {
        return false;
    }

    TArray<int32> Selection = SelectedRealmIndices;
    Selection.Sort();

    // The lock is correct only if it matches the hidden constellation exactly.
    if (Selection != Constellation)
    {
        ++FailedProbes;
        return false;
    }

    // Correct: ignite the true C(K,2) analogies (members correspond under the active
    // relation; decoys do not), driving discoveries/telemetry/audio/scoring through the
    // same path as organic discovery. Idempotent, so a repeat lock awards nothing new.
    Correspond->DetectSharedStructureCorrespondences();
    EvaluateObjective();
    return true;
}

FString UManifoldSlice::GetActiveRelationName() const
{
    switch (ActiveRelation)
    {
    case ECorrespondenceRelation::OctaveInvariant: return TEXT("Octave");
    case ECorrespondenceRelation::Reciprocal:      return TEXT("Reciprocal");
    default:                                        return TEXT("Exact");
    }
}

FString UManifoldSlice::GetRealmSurfaceRatio(int32 Index) const
{
    return RealmSurfaceRatios.IsValidIndex(Index) ? RealmSurfaceRatios[Index] : TEXT("-");
}

FString UManifoldSlice::GetRealmName(int32 Index) const
{
    return ConstellationRealmIds.IsValidIndex(Index) ? ConstellationRealmIds[Index].ToString() : TEXT("-");
}

void UManifoldSlice::HandleIgnited(FGuid /*SourceStructure*/, FGuid /*TargetStructure*/, float Scale)
{
    ++IgnitedCount;

    TMap<FString, FString> Params;
    Params.Add(TEXT("Scale"), FString::SanitizeFloat(Scale));
    Telemetry->LogEvent(TEXT("CorrespondenceIgnited"), CurrentStep, CurrentTime, Params);

    // The discovery is sounded as its ACTUAL ratio (ignition implies Orbits detected
    // a resonance) — the ratio the physics found rings out over the Orbits tonic.
    if (Audio && Orbits)
    {
        const TArray<FResonanceMatch>& Res = Orbits->GetActiveResonances();
        if (Res.Num() > 0)
        {
            LastAudioCue = Audio->CueForDiscovery(TEXT("Orbits"), Res[0].Ratio.X, Res[0].Ratio.Y);
            AudioCues.Add(LastAudioCue);
        }
    }

    // The seam lights up: remember the vortex so it can be transported into Orbits.
    const TArray<FFluidVortex>& Vortices = Fluids->GetActiveVortices();
    if (Vortices.Num() > 0)
    {
        PendingVortexId = Vortices[0].Id;
        bCorrespondenceAvailable = true;

        // Headless / CI: fire immediately. Interactive play waits for the player.
        if (bAutoTransportOnIgnite)
        {
            DoTransportPendingVortex();
        }
    }
}

void UManifoldSlice::HandleSharedDiscovery(FName RealmA, FName /*RealmB*/, FString Ratio, FGuid /*StableId*/)
{
    // A cross-domain analogy (e.g. orbital 3:2 == harmonic 3:2). Count it, record it
    // for the Insight Rate, and sound it as its ACTUAL ratio over the realm's tonic.
    ++SharedDiscoveries;

    int32 Num = 0, Den = 0;
    FString L, R;
    if (Ratio.Split(TEXT(":"), &L, &R))
    {
        Num = FCString::Atoi(*L);
        Den = FCString::Atoi(*R);
    }

    TMap<FString, FString> Params;
    Params.Add(TEXT("Realm"), RealmA.ToString());
    Params.Add(TEXT("Ratio"), Ratio);
    Telemetry->LogEvent(TEXT("Discovery"), CurrentStep, CurrentTime, Params);

    if (Audio && Num > 0 && Den > 0)
    {
        LastAudioCue = Audio->CueForDiscovery(RealmA, Num, Den);
        AudioCues.Add(LastAudioCue);
    }
}

void UManifoldSlice::DoTransportPendingVortex()
{
    if (bCorrespondenceAvailable && PendingVortexId.IsValid())
    {
        Correspond->Transport(PendingVortexId, TEXT("Orbits"));
        bCorrespondenceAvailable = false;
    }
}

bool UManifoldSlice::PlayerRequestTransport()
{
    if (!bCorrespondenceAvailable)
    {
        return false;
    }
    DoTransportPendingVortex();
    return true;
}

void UManifoldSlice::HandleTransport(FGuid /*Source*/, FName TargetRealm, FGuid /*TargetId*/, float /*Strength*/)
{
    ++TransportCount;
    TransportStepLog.Add(static_cast<int32>(CurrentStep));

    TMap<FString, FString> Params;
    Params.Add(TEXT("Realm"), TargetRealm.ToString());
    Telemetry->LogEvent(TEXT("Transport"), CurrentStep, CurrentTime, Params);

    // Crossing the seam resolves the tension toward the destination realm's tonic.
    if (Audio)
    {
        LastAudioCue = Audio->CueForTransport(TEXT("Fluids"), TargetRealm);
        AudioCues.Add(LastAudioCue);
    }
}

void UManifoldSlice::EvaluateObjective()
{
    if (SessionState != EManifoldSessionState::InProgress)
    {
        return; // resolved sessions stay resolved
    }

    const bool bInsightMet = Objective.MinInsightRate <= 0.0f || GetInsightRate() >= Objective.MinInsightRate;
    if (GetTotalDiscoveries() >= Objective.TargetDiscoveries && bInsightMet)
    {
        SessionState = EManifoldSessionState::Won;
    }
    else if (Objective.StepBudget > 0 && CurrentStep >= Objective.StepBudget)
    {
        SessionState = EManifoldSessionState::Lost;
    }
}

FManifoldSessionSummary UManifoldSlice::GetSessionSummary() const
{
    FManifoldSessionSummary Summary;
    Summary.State = SessionState;
    Summary.Discoveries = GetTotalDiscoveries();
    Summary.Transports = TransportCount;
    Summary.InsightRate = GetInsightRate();
    Summary.Steps = CurrentStep;
    Summary.Score = GetScore();
    Summary.Rank = RankForScore(Summary.Score);
    return Summary;
}

int32 UManifoldSlice::GetScore() const
{
    // Discoveries dominate (each is worth 1000), then transports; insight is a modest
    // efficiency bonus (rate, not the raw ~x1000 term that used to swamp everything);
    // plus a speed bonus for winning well under budget.
    int32 Score = GetTotalDiscoveries() * 1000 + TransportCount * 250;
    Score += FMath::RoundToInt(GetInsightRate() * 100.0f);
    if (SessionState == EManifoldSessionState::Won && Objective.StepBudget > 0)
    {
        Score += FMath::Max(0, Objective.StepBudget - static_cast<int32>(CurrentStep)) * 10;
    }
    // Constellation Lock grades difficulty AND precision, so the rank means something:
    // the Octave rule is harder to infer than Exact, and a flawless (no wasted probe)
    // solve is worth a bonus; every wrong lock costs points.
    if (bConstellationMode)
    {
        if (ActiveRelation == ECorrespondenceRelation::OctaveInvariant)
        {
            Score += 2000; // inferring the octave rule is the hard case
        }
        if (SessionState == EManifoldSessionState::Won && FailedProbes == 0)
        {
            Score += 1500; // flawless read of the constellation
        }
        Score -= FailedProbes * 250;
    }
    return FMath::Max(0, Score);
}

EManifoldRank UManifoldSlice::RankForScore(int32 Score)
{
    if (Score >= 9000) return EManifoldRank::S;
    if (Score >= 7000) return EManifoldRank::A;
    if (Score >= 5000) return EManifoldRank::B;
    if (Score >= 3000) return EManifoldRank::C;
    return EManifoldRank::D;
}

void UManifoldSlice::RecordSessionInProfile(FManifoldProfile& Profile, const FManifoldSessionSummary& Summary)
{
    ++Profile.SessionsPlayed;
    if (Summary.State == EManifoldSessionState::Won)
    {
        ++Profile.SessionsWon;
    }
    Profile.BestScore = FMath::Max(Profile.BestScore, Summary.Score);
}

FManifoldReplay UManifoldSlice::CaptureReplay() const
{
    FManifoldReplay Replay;
    Replay.OrbitsSeed = SavedOrbitsSeed;
    Replay.FluidsSeed = SavedFluidsSeed;
    Replay.Steps = static_cast<int32>(CurrentStep);
    Replay.TransportSteps = TransportStepLog;
    Replay.FinalDiscoveries = GetTotalDiscoveries();
    Replay.FinalTransports = TransportCount;
    Replay.FinalInsightRate = GetInsightRate();
    return Replay;
}

bool UManifoldSlice::SaveProfile(const FManifoldProfile& Profile, const FString& Path)
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, /*bIsPersistent*/ true);
    uint32 Magic = FManifoldProfile::Magic;
    uint32 Version = FManifoldProfile::Version;
    Writer << Magic;
    Writer << Version;
    FManifoldProfile Mutable = Profile; // operator<< is non-const
    Writer << Mutable;
    return FFileHelper::SaveArrayToFile(Bytes, *Path);
}

bool UManifoldSlice::LoadProfile(FManifoldProfile& OutProfile, const FString& Path)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        return false;
    }
    FMemoryReader Reader(Bytes, /*bIsPersistent*/ true);
    uint32 Magic = 0;
    uint32 Version = 0;
    Reader << Magic;
    Reader << Version;
    if (Magic != FManifoldProfile::Magic || Version != FManifoldProfile::Version)
    {
        return false;
    }
    // Deserialize into a temporary and only commit on a fully successful read, so a
    // truncated/corrupt file (valid header, short body) can never partially overwrite
    // the caller's profile (which would then be saved back, destroying the real save).
    FManifoldProfile Temp;
    Reader << Temp;
    if (Reader.IsError())
    {
        return false;
    }
    OutProfile = Temp;
    return true;
}

void UManifoldSlice::Tick()
{
    if (!Orbits || !Fluids || !Correspond) return;

    ++CurrentStep;
    Orbits->Step(0.1f);
    Fluids->Step(0.016f);
    if (Harmonics) { Harmonics->Step(0.016f); }
    if (Waves) { Waves->Step(0.001f); }
    if (Rhythm) { Rhythm->Step(0.016f); }
    if (Gears) { Gears->Step(0.016f); }
    if (Decoy) { Decoy->Step(0.016f); }
    CurrentTime = Fluids->GetSimulationTime();

    // Constellation mode resolves through PlayerLockConstellation, NOT automatic
    // detection — auto-firing here would solve the subset-hunt for the player. So the
    // seam + shared-structure auto-detection only run in the classic (non-constellation) mode.
    if (!bConstellationMode)
    {
        // Orbits <-> Fluids (data-driven spec): ignition lights the transport seam.
        Correspond->DetectCorrespondence();

        // Generic N-realm engine: cross-domain analogies by shared structure ratio
        // (Orbits 3:2 <-> Harmonics 3:2). Each newly-found analogy fires
        // OnSharedStructureDiscovered -> HandleSharedDiscovery, which counts it.
        Correspond->DetectSharedStructureCorrespondences();
    }

    // Resolve the session against its objective (win/lose).
    EvaluateObjective();
}

FManifoldSliceResult UManifoldSlice::RunPlaythrough(int32 Steps)
{
    for (int32 i = 0; i < Steps; ++i)
    {
        Tick();
    }

    FManifoldSliceResult Result;
    Result.bResonanceDetected = HasResonance();
    Result.bVortexDetected = HasVortex();
    Result.CorrespondencesIgnited = IgnitedCount;
    Result.TransportsCompleted = TransportCount;
    Result.InsightRate = GetInsightRate();

    Telemetry->ShutdownTelemetry();
    return Result;
}

float UManifoldSlice::GetInsightRate() const
{
    return Telemetry ? Telemetry->CalculateInsightRate() : 0.0f;
}

bool UManifoldSlice::HasResonance() const
{
    return Orbits && Orbits->GetActiveResonances().Num() > 0;
}

bool UManifoldSlice::HasVortex() const
{
    return Fluids && Fluids->GetActiveVortices().Num() > 0;
}

FString UManifoldSlice::GetOrbitsRatio() const
{
    if (Orbits)
    {
        const TArray<FResonanceMatch>& Res = Orbits->GetActiveResonances();
        if (Res.Num() > 0)
        {
            return FString::Printf(TEXT("%d:%d"), Res[0].Ratio.X, Res[0].Ratio.Y);
        }
    }
    return TEXT("-");
}

FString UManifoldSlice::GetHarmonicsRatio() const
{
    if (Harmonics)
    {
        const TArray<FHarmonicRatioMatch>& Ratios = Harmonics->GetActiveRatios();
        if (Ratios.Num() > 0)
        {
            return FString::Printf(TEXT("%d:%d"), Ratios[0].Ratio.X, Ratios[0].Ratio.Y);
        }
    }
    return TEXT("-");
}

FString UManifoldSlice::GetWavesRatio() const
{
    if (Waves)
    {
        const TArray<FWaveRatioMatch>& Ratios = Waves->GetActiveRatios();
        if (Ratios.Num() > 0)
        {
            return FString::Printf(TEXT("%d:%d"), Ratios[0].Ratio.X, Ratios[0].Ratio.Y);
        }
    }
    return TEXT("-");
}

FString UManifoldSlice::GetRhythmRatio() const
{
    if (Rhythm)
    {
        const TArray<FRhythmRatioMatch>& Ratios = Rhythm->GetActiveRatios();
        if (Ratios.Num() > 0)
        {
            return FString::Printf(TEXT("%d:%d"), Ratios[0].Ratio.X, Ratios[0].Ratio.Y);
        }
    }
    return TEXT("-");
}

FString UManifoldSlice::GetGearsRatio() const
{
    if (Gears)
    {
        const TArray<FGearRatioMatch>& Ratios = Gears->GetActiveRatios();
        if (Ratios.Num() > 0)
        {
            return FString::Printf(TEXT("%d:%d"), Ratios[0].Ratio.X, Ratios[0].Ratio.Y);
        }
    }
    return TEXT("-");
}

FString UManifoldSlice::GetDecoyRatio() const
{
    if (Decoy)
    {
        const TArray<FHarmonicRatioMatch>& Ratios = Decoy->GetActiveRatios();
        if (Ratios.Num() > 0)
        {
            return FString::Printf(TEXT("%d:%d"), Ratios[0].Ratio.X, Ratios[0].Ratio.Y);
        }
    }
    return TEXT("-");
}

// ---------------------------------------------------------------------------
// Replay: record a deterministic session, reproduce it, persist it.
// ---------------------------------------------------------------------------

FManifoldReplay UManifoldSlice::RecordReplay(int64 OrbitsSeed, int64 FluidsSeed, int32 Steps)
{
    // Play the session with auto-transport so the schedule is captured naturally.
    Setup(static_cast<uint64>(OrbitsSeed), static_cast<uint64>(FluidsSeed));
    bAutoTransportOnIgnite = true;
    const FManifoldSliceResult Result = RunPlaythrough(Steps);

    FManifoldReplay Replay;
    Replay.OrbitsSeed = static_cast<uint64>(OrbitsSeed);
    Replay.FluidsSeed = static_cast<uint64>(FluidsSeed);
    Replay.Steps = Steps;
    Replay.TransportSteps = TransportStepLog;
    Replay.FinalDiscoveries = GetTotalDiscoveries();
    Replay.FinalTransports = Result.TransportsCompleted;
    Replay.FinalInsightRate = Result.InsightRate;
    return Replay;
}

FManifoldReplay UManifoldSlice::RecordConstellationReplay(int64 Seed, int32 InConstellationSize)
{
    SetupConstellation(Seed, InConstellationSize);
    const TArray<int32> Answer = Constellation; // the correct subset (deterministic)
    PlayerLockConstellation(Answer);

    FManifoldReplay Replay;
    Replay.Mode = 1;
    Replay.OrbitsSeed = static_cast<uint64>(Seed);
    Replay.FluidsSeed = static_cast<uint64>(Seed);
    Replay.Steps = 0;
    Replay.ConstellationSize = ConstellationSize;
    Replay.LockSelection = Answer;
    Replay.FinalDiscoveries = GetTotalDiscoveries();
    Replay.FinalTransports = TransportCount;
    Replay.FinalInsightRate = GetInsightRate();
    return Replay;
}

FManifoldSliceResult UManifoldSlice::RunReplay(const FManifoldReplay& Replay)
{
    // Constellation replay: rebuild the deterministic puzzle from the seed and re-lock the
    // recorded subset. SetupConstellation + PlayerLockConstellation reproduce it exactly.
    if (Replay.Mode == 1)
    {
        SetupConstellation(static_cast<int64>(Replay.OrbitsSeed), Replay.ConstellationSize);
        PlayerLockConstellation(Replay.LockSelection);

        FManifoldSliceResult Result;
        Result.bResonanceDetected = HasResonance();
        Result.bVortexDetected = HasVortex();
        Result.CorrespondencesIgnited = IgnitedCount;
        Result.TransportsCompleted = TransportCount;
        Result.InsightRate = GetInsightRate();
        if (Telemetry) { Telemetry->ShutdownTelemetry(); }
        return Result;
    }

    // Faithful reproduction that HONORS the recorded transport schedule: the player's
    // verb is re-issued on exactly the recorded steps (not auto-fired), so the run is
    // reproduced the way it was actually played. Determinism guarantees the same seeds
    // + same schedule => the same result, bit-for-bit.
    Setup(Replay.OrbitsSeed, Replay.FluidsSeed);
    bAutoTransportOnIgnite = false;

    // How many transports the player issued on each recorded step (usually 1).
    TMap<int32, int32> TransportsPerStep;
    for (int32 Step : Replay.TransportSteps)
    {
        TransportsPerStep.FindOrAdd(Step)++;
    }

    for (int32 i = 0; i < Replay.Steps; ++i)
    {
        Tick();
        if (const int32* Count = TransportsPerStep.Find(static_cast<int32>(CurrentStep)))
        {
            for (int32 n = 0; n < *Count; ++n)
            {
                if (!PlayerRequestTransport())
                {
                    break; // no correspondence available to consume
                }
            }
        }
    }

    FManifoldSliceResult Result;
    Result.bResonanceDetected = HasResonance();
    Result.bVortexDetected = HasVortex();
    Result.CorrespondencesIgnited = IgnitedCount;
    Result.TransportsCompleted = TransportCount;
    Result.InsightRate = GetInsightRate();

    Telemetry->ShutdownTelemetry();
    return Result;
}

bool UManifoldSlice::SaveReplay(const FManifoldReplay& Replay, const FString& Path)
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, /*bIsPersistent*/ true);
    uint32 Magic = FManifoldReplay::Magic;
    uint32 Version = FManifoldReplay::Version;
    Writer << Magic;
    Writer << Version;
    FManifoldReplay Mutable = Replay; // operator<< is non-const
    Writer << Mutable;
    return FFileHelper::SaveArrayToFile(Bytes, *Path);
}

bool UManifoldSlice::LoadReplay(FManifoldReplay& OutReplay, const FString& Path)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        return false;
    }

    FMemoryReader Reader(Bytes, /*bIsPersistent*/ true);
    uint32 Magic = 0;
    uint32 Version = 0;
    Reader << Magic;
    Reader << Version;
    if (Magic != FManifoldReplay::Magic || Version != FManifoldReplay::Version)
    {
        return false; // not ours / wrong version — reject rather than misread
    }
    // Read into a temporary; only commit on a fully successful read (a truncated file
    // must not partially overwrite the caller's replay).
    FManifoldReplay Temp;
    Reader << Temp;
    if (Reader.IsError())
    {
        return false;
    }
    OutReplay = Temp;
    return true;
}
