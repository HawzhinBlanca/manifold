// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
#include "RhythmKernel.h"
#include "CorrespondenceSystem.h"
#include "TelemetrySystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

void UManifoldSlice::Setup(uint64 OrbitsSeed, uint64 FluidsSeed)
{
    Orbits = NewObject<UOrbitsKernel>(this);
    Fluids = NewObject<UFluidsKernel>(this);
    Harmonics = NewObject<UHarmonicsKernel>(this);
    Waves = NewObject<UWavesKernel>(this);
    Rhythm = NewObject<URhythmKernel>(this);
    Correspond = NewObject<UCorrespondenceSystem>(this);
    Telemetry = NewObject<UTelemetrySystem>(this);
    Audio = NewObject<UManifoldAudioDirector>(this);

    Orbits->Initialize(OrbitsSeed);
    Fluids->Initialize(FluidsSeed);
    Harmonics->Initialize(OrbitsSeed ^ 0xABCDEF);
    Waves->Initialize(OrbitsSeed ^ 0x123456);
    Rhythm->Initialize(OrbitsSeed ^ 0x789ABC);
    Correspond->RegisterKernels(Orbits, Fluids);

    // Generic N-realm engine: any realms exposing the same ratio correspond.
    // A 3:2 now shows up across FIVE domains — celestial, acoustic, spatial, and
    // temporal — the cross-domain analogy that is the heart of MANIFOLD.
    Correspond->RegisterRealm(TEXT("Orbits"), TEXT("OrbitalResonance"), Orbits);
    Correspond->RegisterRealm(TEXT("Harmonics"), TEXT("HarmonicRatio"), Harmonics);
    Correspond->RegisterRealm(TEXT("Waves"), TEXT("WaveHarmonic"), Waves);
    Correspond->RegisterRealm(TEXT("Rhythm"), TEXT("RhythmRatio"), Rhythm);

    Telemetry->InitializeTelemetry(TEXT("SlicePlaythrough.log"));

    // The PUZZLE: this session's realms all secretly share ONE hidden ratio, chosen
    // deterministically from the seed. Different seeds hide different ratios (3:2, 4:3,
    // 5:3, 2:1, ...), so no two sessions are the same and the game can't be pre-solved
    // — the design's "un-pre-computable" pillar, made real.
    PickSharedRatio(OrbitsSeed, SharedP, SharedQ);
    const double RatioD = static_cast<double>(SharedP) / static_cast<double>(SharedQ);

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
    return Summary;
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
    CurrentTime = Fluids->GetSimulationTime();

    // Orbits <-> Fluids (data-driven spec): ignition lights the transport seam.
    Correspond->DetectCorrespondence();

    // Generic N-realm engine: cross-domain analogies by shared structure ratio
    // (Orbits 3:2 <-> Harmonics 3:2). Each newly-found analogy fires
    // OnSharedStructureDiscovered -> HandleSharedDiscovery, which counts it.
    Correspond->DetectSharedStructureCorrespondences();

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

FManifoldSliceResult UManifoldSlice::RunReplay(const FManifoldReplay& Replay)
{
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
    Reader << OutReplay;
    return !Reader.IsError();
}
