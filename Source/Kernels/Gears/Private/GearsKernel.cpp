// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "GearsKernel.h"
#include "UObject/UObjectGlobals.h"

UGearsKernel::UGearsKernel()
{
    GState = MakeShared<FGearsState>();
}

void UGearsKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    GState = MakeShared<FGearsState>();
    GState->RealmId = GetRealmId();
    GState->SimulationVersion = GetSimulationVersion();
    GState->Seed = Seed;
    GState->SimulationTime = 0.0;
    GState->StepCount = 0;
}

void UGearsKernel::Reset()
{
    Initialize(GState->Seed);
}

void UGearsKernel::Shutdown()
{
    GState.Reset();
}

void UGearsKernel::Step(float DeltaTime)
{
    const double dt = static_cast<double>(DeltaTime);

    for (FGear& Gear : GState->Gears)
    {
        // Meshed gears turn at a speed inversely proportional to their tooth count.
        const double Speed = (Gear.Teeth > 0) ? (1.0 / static_cast<double>(Gear.Teeth)) : 0.0;
        Gear.Angle += 2.0 * PI * Speed * dt;
        Gear.Angle = FMath::Fmod(Gear.Angle, 2.0 * PI);
        if (Gear.Angle < 0.0)
        {
            Gear.Angle += 2.0 * PI;
        }
    }

    SimTime += dt;
    StepCount++;
    GState->SimulationTime = SimTime;
    GState->StepCount = StepCount;

    UpdateDerivedState();
}

void UGearsKernel::StepMultiple(float DeltaTime, int32 NumSteps)
{
    for (int32 i = 0; i < NumSteps; ++i)
    {
        Step(DeltaTime);
    }
}

void UGearsKernel::UpdateDerivedState()
{
    DetectGearRatios();
    GState->StateHash = ComputeStateHash();
}

TSharedPtr<FRealmState> UGearsKernel::GetState() const
{
    return GState;
}

void UGearsKernel::SetState(const FRealmState& NewState)
{
    const FGearsState* Casted = static_cast<const FGearsState*>(&NewState);
    if (!Casted) return;

    GState = MakeShared<FGearsState>(*Casted);
    SimTime = GState->SimulationTime;
    StepCount = GState->StepCount;
}

void UGearsKernel::SerializeState(FArchive& Ar)
{
    if (GState.IsValid())
    {
        Ar << *GState;
    }
}

void UGearsKernel::DeserializeState(FArchive& Ar)
{
    FGearsState Loaded;
    Ar << Loaded;
    SetState(Loaded);
}

uint64 UGearsKernel::ComputeStateHash() const
{
    uint64 Hash = 0x1F83D9ABFB41BD6BULL;
    auto HashDouble = [](double Val) -> uint64
    {
        uint64 Bits;
        FMemory::Memcpy(&Bits, &Val, sizeof(double));
        return Bits;
    };

    for (const FGear& Gear : GState->Gears)
    {
        Hash ^= static_cast<uint64>(Gear.Teeth) * 0x9E3779B97F4A7C15ULL;
        Hash ^= HashDouble(Gear.Angle) * 0xBF58476D1CE4E5B9ULL;
    }
    return Hash;
}

bool UGearsKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("GearRatio"))
    {
        const float MinStr = QueryObj.MinStrength;
        const FGearRatioMatch* Best = nullptr;
        for (const FGearRatioMatch& Match : GState->ActiveRatios)
        {
            if (Match.Strength < MinStr) continue;
            if (!Best || Match.Strength > Best->Strength)
            {
                Best = &Match;
            }
        }

        if (Best)
        {
            // Stable identity: same realm + same detected ratio => same id across frames.
            Result.StructureId = FGuid(GetTypeHash(GetRealmId()),
                static_cast<uint32>(Best->Ratio.X), static_cast<uint32>(Best->Ratio.Y),
                GetSimulationVersion());
            Result.StructureType = QueryObj.QueryType;
            Result.Strength = static_cast<float>(Best->Strength);
            Result.Parameters.Add(TEXT("Ratio"), FString::Printf(TEXT("%d:%d"), Best->Ratio.X, Best->Ratio.Y));
            return true;
        }
    }
    return false;
}

void UGearsKernel::QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const
{
    if (QueryDesc.QueryType != TEXT("GearRatio")) return;

    const float MinStr = QueryDesc.MinStrength;
    for (const FGearRatioMatch& Match : GState->ActiveRatios)
    {
        if (Match.Strength < MinStr) continue;

        FRealmQueryResult Result;
        Result.StructureId = FGuid(GetTypeHash(GetRealmId()),
            static_cast<uint32>(Match.Ratio.X), static_cast<uint32>(Match.Ratio.Y),
            GetSimulationVersion());
        Result.StructureType = QueryDesc.QueryType;
        Result.Strength = static_cast<float>(Match.Strength);
        Result.Parameters.Add(TEXT("Ratio"), FString::Printf(TEXT("%d:%d"), Match.Ratio.X, Match.Ratio.Y));
        OutResults.Add(Result);
    }
}

TArray<FName> UGearsKernel::GetSupportedQueryTypes() const
{
    return { TEXT("GearRatio") };
}

TMap<FName, FString> UGearsKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("GearCount"), TEXT("int"));
    return Schema;
}

bool UGearsKernel::SetParameter(FName /*Name*/, const FString& /*Value*/)
{
    return false;
}

bool UGearsKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("GearCount"))
    {
        OutValue = FString::FromInt(GState->Gears.Num());
        return true;
    }
    return false;
}

bool UGearsKernel::VerifyDeterminism(int32 NumSteps) const
{
    UGearsKernel* SimA = NewObject<UGearsKernel>();
    UGearsKernel* SimB = NewObject<UGearsKernel>();
    SimA->Initialize(GState->Seed);
    SimB->Initialize(GState->Seed);

    for (const FGear& Gear : GState->Gears)
    {
        SimA->AddGear(Gear.Teeth);
        SimB->AddGear(Gear.Teeth);
    }

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.01f);
        SimB->Step(0.01f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

FGuid UGearsKernel::AddGear(int32 Teeth)
{
    FGear Gear;
    Gear.Id = FGuid::NewGuid();
    Gear.Teeth = FMath::Max(1, Teeth);
    Gear.Angle = 0.0;
    GState->Gears.Add(Gear);

    UpdateDerivedState();
    return Gear.Id;
}

const TArray<FGearRatioMatch>& UGearsKernel::GetActiveRatios() const
{
    return GState->ActiveRatios;
}

void UGearsKernel::DetectGearRatios()
{
    GState->ActiveRatios.Empty();

    const TArray<FGear>& Gears = GState->Gears;
    for (int32 i = 0; i < Gears.Num(); ++i)
    {
        for (int32 j = i + 1; j < Gears.Num(); ++j)
        {
            const int32 Ta = Gears[i].Teeth;
            const int32 Tb = Gears[j].Teeth;
            if (Ta <= 0 || Tb <= 0) continue;

            const int32 Hi = FMath::Max(Ta, Tb);
            const int32 Lo = FMath::Min(Ta, Tb);
            const int32 G = FMath::GreatestCommonDivisor(Hi, Lo);
            if (G <= 0) continue;

            FGearRatioMatch Match;
            Match.Ratio = FIntPoint(Hi / G, Lo / G); // exact, reduced
            Match.Strength = 1.0;
            GState->ActiveRatios.Add(Match);
        }
    }
}
