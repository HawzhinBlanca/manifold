// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "CorrespondenceSystem.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"

UCorrespondenceSystem::UCorrespondenceSystem()
{
    OrbitsKernel = nullptr;
    FluidsKernel = nullptr;
    Mapping = nullptr;
}

void UCorrespondenceSystem::RegisterKernels(UObject* InOrbitsKernel, UObject* InFluidsKernel)
{
    OrbitsKernel = InOrbitsKernel;
    FluidsKernel = InFluidsKernel;
}

void UCorrespondenceSystem::InitializeMapping(UCorrespondenceMapping* MappingAsset)
{
    Mapping = MappingAsset;
}

bool UCorrespondenceSystem::DetectCorrespondence()
{
    if (!OrbitsKernel || !FluidsKernel) return false;

    IRealmKernel* Orbits = Cast<IRealmKernel>(OrbitsKernel);
    IRealmKernel* Fluids = Cast<IRealmKernel>(FluidsKernel);
    if (!Orbits || !Fluids) return false;

    // Build data-driven queries
    FRealmQuery OrbitsQuery(TEXT("OrbitalResonance"));
    FRealmQueryResult OrbitsResult;
    bool bOrbitsFound = Orbits->Query(OrbitsQuery, OrbitsResult);

    FRealmQuery FluidsQuery(TEXT("VortexCenter"));
    FRealmQueryResult FluidsResult;
    bool bFluidsFound = Fluids->Query(FluidsQuery, FluidsResult);

    if (bOrbitsFound && bFluidsFound)
    {
        // Check specs
        if (Mapping && Mapping->Specs.Num() > 0)
        {
            for (const FCorrespondenceSpec& Spec : Mapping->Specs)
            {
                if (OrbitsResult.StructureType == Spec.SourceStructureType &&
                    FluidsResult.StructureType == Spec.TargetStructureType)
                {
                    FString RatioVal = OrbitsResult.Parameters.FindRef(TEXT("Ratio"));
                    if (RatioVal == Spec.MatchingRatio)
                    {
                        if (!IgnitedCorrespondences.Contains(OrbitsResult.StructureId))
                        {
                            IgnitedCorrespondences.Add(OrbitsResult.StructureId);
                            OnCorrespondenceIgnited.Broadcast(OrbitsResult.StructureId, FluidsResult.StructureId, Spec.ScaleFactor);
                            return true;
                        }
                    }
                }
            }
        }
        else
        {
            // Fallback default mapping when spec asset is not provided
            if (OrbitsResult.Parameters.FindRef(TEXT("Ratio")) == TEXT("3:2"))
            {
                if (!IgnitedCorrespondences.Contains(OrbitsResult.StructureId))
                {
                    IgnitedCorrespondences.Add(OrbitsResult.StructureId);
                    OnCorrespondenceIgnited.Broadcast(OrbitsResult.StructureId, FluidsResult.StructureId, 1.0f);
                    return true;
                }
            }
        }
    }

    return false;
}

bool UCorrespondenceSystem::Transport(FGuid SourceStructureId, FName TargetRealm)
{
    if (!OrbitsKernel || !FluidsKernel) return false;

    UOrbitsKernel* Orbits = Cast<UOrbitsKernel>(OrbitsKernel);
    UFluidsKernel* Fluids = Cast<UFluidsKernel>(FluidsKernel);
    if (!Orbits || !Fluids) return false;

    if (TargetRealm == TEXT("Fluids"))
    {
        // Transport Orbit body perturbation to Fluids vortex
        const FOrbitsBodyDef* Body = Orbits->GetBody(SourceStructureId);
        if (Body)
        {
            // Normalize orbital coordinates to [0..1] range
            // Orbit position scales around 10^11 meters, normalize using semi-major axis scale
            FOrbitalElements Elem;
            if (Orbits->GetOrbitalElements(SourceStructureId, Elem))
            {
                double SemiMajor = Elem.SemiMajorAxis;
                if (SemiMajor > 0.0)
                {
                    float NormX = static_cast<float>((Body->Position.X / SemiMajor + 1.0) * 0.5);
                    float NormY = static_cast<float>((Body->Position.Y / SemiMajor + 1.0) * 0.5);

                    NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
                    NormY = FMath::Clamp(NormY, 0.0f, 1.0f);

                    float Strength = static_cast<float>(Elem.MeanMotion * Body->Mass * 1e-24); // scaled

                    Fluids->AddDensity(NormX, NormY, 1.0f);
                    Fluids->AddVelocity(NormX, NormY, static_cast<float>(Body->Velocity.X * 1e-4), static_cast<float>(Body->Velocity.Y * 1e-4));

                    FGuid TargetId = FGuid::NewGuid();
                    OnTransportCompleted.Broadcast(SourceStructureId, TargetRealm, TargetId, Strength);
                    return true;
                }
            }
        }
    }
    else if (TargetRealm == TEXT("Orbits"))
    {
        // Transport Fluids vortex to Orbits orbital body
        // Find matching vortex
        const TArray<FFluidVortex>& Vortices = Fluids->GetActiveVortices();
        for (const FFluidVortex& Vor : Vortices)
        {
            if (Vor.Id == SourceStructureId)
            {
                // Create a planet at corresponding position
                FOrbitsBodyDef Planet;
                Planet.Name = TEXT("TransportedPlanet");
                Planet.Mass = FMath::Abs(Vor.Strength) * 1e24; // map strength to mass
                
                // Map normalized coords back to Orbits space
                float NormX = Vor.Position.X / 1000.0f;
                float NormY = Vor.Position.Y / 1000.0f;
                
                double OrbitScale = 1.496e11; // 1 AU
                Planet.Position = FVector((NormX * 2.0 - 1.0) * OrbitScale, (NormY * 2.0 - 1.0) * OrbitScale, 0.0);
                
                // Circular orbit velocity: v = sqrt(G * M / r)
                double Dist = Planet.Position.Size();
                if (Dist > 0.0)
                {
                    double V_circ = FMath::Sqrt((Orbits->G * 1.989e30) / Dist);
                    Planet.Velocity = FVector(-Planet.Position.Y, Planet.Position.X, 0.0).GetSafeNormal() * V_circ;
                }
                
                Planet.Radius = 6371000.0; // Earth size
                Planet.Color = FLinearColor::Green;
                Planet.bIsCentral = false;

                FGuid TargetId = Orbits->AddBody(Planet);
                OnTransportCompleted.Broadcast(SourceStructureId, TargetRealm, TargetId, Vor.Strength);
                return true;
            }
        }
    }

    return false;
}
