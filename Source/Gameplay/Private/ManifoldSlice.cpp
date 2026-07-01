// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldSlice.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "CorrespondenceSystem.h"
#include "TelemetrySystem.h"
#include "Misc/Paths.h"

void UManifoldSlice::Setup(uint64 OrbitsSeed, uint64 FluidsSeed)
{
    Orbits = NewObject<UOrbitsKernel>(this);
    Fluids = NewObject<UFluidsKernel>(this);
    Correspond = NewObject<UCorrespondenceSystem>(this);
    Telemetry = NewObject<UTelemetrySystem>(this);

    Orbits->Initialize(OrbitsSeed);
    Fluids->Initialize(FluidsSeed);
    Correspond->RegisterKernels(Orbits, Fluids);
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

    TMap<FString, FString> Params;
    Params.Add(TEXT("Realm"), TargetRealm.ToString());
    Telemetry->LogEvent(TEXT("Transport"), CurrentStep, CurrentTime, Params);
}

void UManifoldSlice::Tick()
{
    if (!Orbits || !Fluids || !Correspond) return;

    ++CurrentStep;
    Orbits->Step(0.1f);
    Fluids->Step(0.016f);
    CurrentTime = Fluids->GetSimulationTime();

    // Look across the seam every step; ignition fires the reactions above.
    Correspond->DetectCorrespondence();
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
