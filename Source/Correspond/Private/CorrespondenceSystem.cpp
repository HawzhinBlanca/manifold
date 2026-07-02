// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "CorrespondenceSystem.h"
#include "OrbitsKernel.h"
#include "FluidsKernel.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"

UCorrespondenceMapping* UCorrespondenceMapping::CreateFromJsonString(const FString& JsonText, UObject* Outer)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return nullptr;
    }

    UCorrespondenceMapping* Mapping = NewObject<UCorrespondenceMapping>(Outer ? Outer : GetTransientPackage());

    const TArray<TSharedPtr<FJsonValue>>* SpecArray = nullptr;
    if (Root->TryGetArrayField(TEXT("specs"), SpecArray) && SpecArray)
    {
        for (const TSharedPtr<FJsonValue>& Value : *SpecArray)
        {
            const TSharedPtr<FJsonObject>* Obj = nullptr;
            if (!Value->TryGetObject(Obj) || !Obj) continue;

            FCorrespondenceSpec Spec;
            Spec.SourceStructureType = FName(*(*Obj)->GetStringField(TEXT("sourceStructureType")));
            Spec.TargetStructureType = FName(*(*Obj)->GetStringField(TEXT("targetStructureType")));
            Spec.MatchingRatio = (*Obj)->GetStringField(TEXT("matchingRatio"));

            double Tolerance = 0.05;
            (*Obj)->TryGetNumberField(TEXT("tolerance"), Tolerance);
            Spec.Tolerance = static_cast<float>(Tolerance);

            double ScaleFactor = 1.0;
            (*Obj)->TryGetNumberField(TEXT("scaleFactor"), ScaleFactor);
            Spec.ScaleFactor = static_cast<float>(ScaleFactor);

            Mapping->Specs.Add(Spec);
        }
    }

    return Mapping;
}

UCorrespondenceMapping* UCorrespondenceMapping::CreateFromJsonFile(const FString& FilePath, UObject* Outer)
{
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
    {
        return nullptr;
    }
    return CreateFromJsonString(JsonText, Outer);
}

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

void UCorrespondenceSystem::RegisterRealm(FName RealmId, FName StructureQueryType, UObject* Kernel)
{
    FRegisteredRealm Realm;
    Realm.RealmId = RealmId;
    Realm.StructureQueryType = StructureQueryType;
    Realm.Kernel = Kernel;
    RegisteredRealms.Add(Realm);
}

FString UCorrespondenceSystem::NormalizeRatio(const FString& Ratio, ECorrespondenceRelation Relation)
{
    FString LeftStr, RightStr;
    if (!Ratio.Split(TEXT(":"), &LeftStr, &RightStr))
    {
        return Ratio; // not a "p:q" ratio — leave untouched
    }

    int32 P = FCString::Atoi(*LeftStr);
    int32 Q = FCString::Atoi(*RightStr);
    if (P <= 0 || Q <= 0)
    {
        return Ratio; // non-positive / unparsable — leave untouched
    }

    // Octave-invariance: divide out all factors of 2 from each term BEFORE reducing,
    // so period-doublings collapse into one class (3:2, 6:4, 3:1 -> 3:1).
    //
    // DIRECTIONAL BY DESIGN — do NOT canonicalize term order here (unlike the Reciprocal
    // branch below). Ratios in this game are directional and emitted Hi:Lo (p >= q) by every
    // kernel, and the octave DECOYS rely on order staying significant: 4:3 -> "1:3" must stay
    // distinct from the base-3 member class 3:1 -> "3:1" (and 8:5 -> "1:5" from 5:1). Folding
    // order to min:max would collapse those decoys onto the member classes and silently break
    // constellation discrimination — see the reject filter in ManifoldSlice.cpp and the
    // regression test MANIFOLD.Correspondence.OctaveDecoyDistinctness. (Reciprocal is the
    // separate, order-independent relation.)
    if (Relation == ECorrespondenceRelation::OctaveInvariant)
    {
        while ((P & 1) == 0) { P >>= 1; }
        while ((Q & 1) == 0) { Q >>= 1; }
    }

    // Reduce to lowest terms (the canonical form for Exact; also tidies the others).
    auto GcdOf = [](int32 A, int32 B)
    {
        while (B != 0) { const int32 T = A % B; A = B; B = T; }
        return A < 0 ? -A : A;
    };
    const int32 G = GcdOf(P, Q);
    if (G > 0) { P /= G; Q /= G; }

    // Reciprocity: p:q corresponds to q:p, so collapse order to a canonical min:max key.
    if (Relation == ECorrespondenceRelation::Reciprocal)
    {
        const int32 Lo = FMath::Min(P, Q);
        const int32 Hi = FMath::Max(P, Q);
        P = Lo; Q = Hi;
    }

    return FString::Printf(TEXT("%d:%d"), P, Q);
}

int32 UCorrespondenceSystem::DetectSharedStructureCorrespondences()
{
    // Ask each realm for ALL its structure ratios (not just the strongest), so two
    // realms that share ANY ratio correspond — even a realm that exposes several.
    TArray<TPair<FName, FString>> RealmRatios;
    for (const FRegisteredRealm& Realm : RegisteredRealms)
    {
        IRealmKernel* Kernel = Cast<IRealmKernel>(Realm.Kernel);
        if (!Kernel) continue;

        FRealmQuery Query(Realm.StructureQueryType);
        TArray<FRealmQueryResult> Results;
        Kernel->QueryAll(Query, Results);
        for (const FRealmQueryResult& Result : Results)
        {
            const FString Ratio = Result.Parameters.FindRef(TEXT("Ratio"));
            if (!Ratio.IsEmpty())
            {
                RealmRatios.Add(TPair<FName, FString>(Realm.RealmId, Ratio));
            }
        }
    }

    // Any two DIFFERENT realms whose ratios correspond UNDER THE ACTIVE RELATION match
    // across the seam. Under Exact this is literal equality (today's behavior); under
    // Octave/Reciprocal, realms that look different on the surface (6:4 vs 3:2 vs 3:1)
    // can still correspond — the Constellation Lock puzzle. The normalized form is the
    // shared identity used for the compare, the dedup key, and the broadcast.
    int32 NewCount = 0;
    for (int32 i = 0; i < RealmRatios.Num(); ++i)
    {
        const FString NormI = NormalizeRatio(RealmRatios[i].Value, ActiveRelation);
        for (int32 j = i + 1; j < RealmRatios.Num(); ++j)
        {
            if (RealmRatios[i].Key == RealmRatios[j].Key) continue;  // same realm
            const FString NormJ = NormalizeRatio(RealmRatios[j].Value, ActiveRelation);
            if (NormI != NormJ) continue;  // don't correspond under the active relation

            const FString Key = FString::Printf(TEXT("%s|%s|%s"),
                *RealmRatios[i].Key.ToString(),
                *RealmRatios[j].Key.ToString(),
                *NormI);

            if (!IgnitedSharedStructures.Contains(Key))
            {
                IgnitedSharedStructures.Add(Key);
                // Stable identity for the shared structure: deterministic from the
                // (order-independent) realm pair + normalized ratio, so the same analogy
                // always has the same id (no throwaway GUIDs). Hash the realm-id STRINGS
                // (content-stable) rather than the FName handles (whose GetTypeHash is the
                // process-local name-table index — not reproducible across runs/platforms).
                const uint32 HashA = GetTypeHash(RealmRatios[i].Key.ToString());
                const uint32 HashB = GetTypeHash(RealmRatios[j].Key.ToString());
                const uint32 RatioHash = GetTypeHash(NormI);
                const FGuid StableId(HashA ^ HashB, HashA + HashB, RatioHash, 0x5AA5u);
                OnSharedStructureDiscovered.Broadcast(
                    RealmRatios[i].Key, RealmRatios[j].Key, NormI, StableId);
                ++NewCount;
            }
        }
    }
    return NewCount;
}

void UCorrespondenceSystem::InitializeMapping(UCorrespondenceMapping* MappingAsset)
{
    Mapping = MappingAsset;
}

void UCorrespondenceSystem::EnsureDefaultMapping()
{
    if (Mapping)
    {
        // An explicit mapping was installed (even a deliberately EMPTY one, meaning
        // "no correspondences") — respect it rather than overriding with the default.
        return;
    }

    // No content supplied: build the canonical default as real spec DATA so the
    // matching path is always data-driven (no magic inline string comparison).
    Mapping = NewObject<UCorrespondenceMapping>(this);
    FCorrespondenceSpec Spec;
    Spec.SourceStructureType = TEXT("OrbitalResonance");
    Spec.TargetStructureType = TEXT("VortexCenter");
    Spec.MatchingRatio = TEXT("3:2");
    Spec.Tolerance = 0.05f;
    Spec.ScaleFactor = 1.0f;
    Mapping->Specs.Add(Spec);
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

    if (!bOrbitsFound || !bFluidsFound)
    {
        return false;
    }

    // Detection is ALWAYS spec-driven (a default spec is synthesized if no content
    // asset was provided), and honors each spec's numeric Tolerance.
    EnsureDefaultMapping();

    const FString RatioVal = OrbitsResult.Parameters.FindRef(TEXT("Ratio"));
    const FString DevStr = OrbitsResult.Parameters.FindRef(TEXT("Deviation"));
    const float Deviation = DevStr.IsEmpty() ? 0.0f : FCString::Atof(*DevStr);

    for (const FCorrespondenceSpec& Spec : Mapping->Specs)
    {
        const bool bTypesMatch =
            OrbitsResult.StructureType == Spec.SourceStructureType &&
            FluidsResult.StructureType == Spec.TargetStructureType;
        const bool bRatioMatch = RatioVal == Spec.MatchingRatio;
        const bool bWithinTolerance = Deviation <= Spec.Tolerance;

        if (bTypesMatch && bRatioMatch && bWithinTolerance)
        {
            if (!IgnitedCorrespondences.Contains(OrbitsResult.StructureId))
            {
                IgnitedCorrespondences.Add(OrbitsResult.StructureId);
                OnCorrespondenceIgnited.Broadcast(OrbitsResult.StructureId, FluidsResult.StructureId, Spec.ScaleFactor);
                return true;
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

                    // Deterministic, traceable id for the injected perturbation (was a
                    // throwaway GUID): derived from the source structure + target realm +
                    // where in the field it landed, so the same transport reproduces it.
                    const FGuid TargetId(
                        SourceStructureId.A,
                        GetTypeHash(TargetRealm),
                        static_cast<uint32>(FMath::RoundToInt(NormX * 100000.0f)),
                        static_cast<uint32>(FMath::RoundToInt(NormY * 100000.0f)));
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
