// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "HarmonicsKernel.h"
#include "WavesKernel.h"
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
    Correspond = NewObject<UCorrespondenceSystem>(this);
    Telemetry = NewObject<UTelemetrySystem>(this);

    Orbits->Initialize(OrbitsSeed);
    Fluids->Initialize(FluidsSeed);
    Harmonics->Initialize(OrbitsSeed ^ 0xABCDEF);
    Waves->Initialize(OrbitsSeed ^ 0x123456);
    Correspond->RegisterKernels(Orbits, Fluids);

    // Generic N-realm engine: any realms exposing the same ratio correspond.
    // A 3:2 shows up across THREE domains — celestial, acoustic, spatial — the
    // cross-domain analogy that is the heart of MANIFOLD.
    Correspond->RegisterRealm(TEXT("Orbits"), TEXT("OrbitalResonance"), Orbits);
    Correspond->RegisterRealm(TEXT("Harmonics"), TEXT("HarmonicRatio"), Harmonics);
    Correspond->RegisterRealm(TEXT("Waves"), TEXT("WaveHarmonic"), Waves);

    Telemetry->InitializeTelemetry(TEXT("SlicePlaythrough.log"));

    // Load the data-driven correspondence content (Build Plan D1). If the content
    // file is missing, the CorrespondenceSystem falls back to its built-in "3:2" rule.
    const FString MappingPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Correspondences/OrbitsFluids.json"));
    if (UCorrespondenceMapping* Mapping = UCorrespondenceMapping::CreateFromJsonFile(MappingPath, this))
    {
        Correspond->InitializeMapping(Mapping);
    }

    // Gameplay reactions: on a detected correspondence, transport power across the
    // seam; record every discovery/transport for the Insight Rate.
    Correspond->OnCorrespondenceIgnited.AddUObject(this, &UManifoldSlice::HandleIgnited);
    Correspond->OnTransportCompleted.AddUObject(this, &UManifoldSlice::HandleTransport);

    // --- Orbits scenario: a clean 3:2 mean-motion resonance ---
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

    const double R_B = PlanetA.Position.X * FMath::Pow(1.5, 2.0 / 3.0); // period ratio 3:2
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

    // --- Harmonics scenario: modes at 2 Hz and 3 Hz form a 3:2 (same structure
    //     as the orbital resonance — the cross-domain analogy). ---
    Harmonics->AddMode(2.0, 1.0);
    Harmonics->AddMode(3.0, 1.0);

    // --- Waves scenario: the 2nd and 3rd string harmonics form a 3:2 as well. ---
    Waves->ExciteHarmonic(2, 1.0);
    Waves->ExciteHarmonic(3, 1.0);
}

void UManifoldSlice::HandleIgnited(FGuid /*SourceStructure*/, FGuid /*TargetStructure*/, float Scale)
{
    ++IgnitedCount;

    TMap<FString, FString> Params;
    Params.Add(TEXT("Scale"), FString::SanitizeFloat(Scale));
    Telemetry->LogEvent(TEXT("CorrespondenceIgnited"), CurrentStep, CurrentTime, Params);

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
    CurrentTime = Fluids->GetSimulationTime();

    // Orbits <-> Fluids (data-driven spec): ignition lights the transport seam.
    Correspond->DetectCorrespondence();

    // Generic N-realm engine: cross-domain analogies by shared structure ratio
    // (Orbits 3:2 <-> Harmonics 3:2). Each new one is a discovery.
    SharedDiscoveries += Correspond->DetectSharedStructureCorrespondences();

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
    // Deterministic reproduction: the same seeds and step count re-run the exact
    // session bit-for-bit — the "share a seed, get the identical run" guarantee.
    // (The player-driven transport verb is exercised separately by the interactive
    // session test; here the point is faithful, reproducible playback.)
    Setup(Replay.OrbitsSeed, Replay.FluidsSeed);
    bAutoTransportOnIgnite = true;
    return RunPlaythrough(Replay.Steps);
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
