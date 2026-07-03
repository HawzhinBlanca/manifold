// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "CircuitsKernel.h"
#include "UObject/UObjectGlobals.h"

UCircuitsKernel::UCircuitsKernel()
{
    CState = MakeShared<FCircuitsState>();
}

void UCircuitsKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    CState = MakeShared<FCircuitsState>();
    CState->RealmId = GetRealmId();
    CState->SimulationVersion = GetSimulationVersion();
    CState->Seed = Seed;
    CState->SimulationTime = 0.0;
    CState->StepCount = 0;
}

void UCircuitsKernel::Reset()
{
    Initialize(CState->Seed);
}

void UCircuitsKernel::Shutdown()
{
    CState.Reset();
}

void UCircuitsKernel::Step(float DeltaTime)
{
    const double dt = static_cast<double>(DeltaTime);

    for (FResonantTank& Tank : CState->Tanks)
    {
        // Charge oscillates at the tank's resonant frequency.
        Tank.Phase += 2.0 * PI * Tank.Frequency * dt;
        Tank.Phase = FMath::Fmod(Tank.Phase, 2.0 * PI);
        if (Tank.Phase < 0.0)
        {
            Tank.Phase += 2.0 * PI;
        }
    }

    SimTime += dt;
    StepCount++;
    CState->SimulationTime = SimTime;
    CState->StepCount = StepCount;

    UpdateDerivedState();
}

void UCircuitsKernel::UpdateDerivedState()
{
    DetectCircuitRatios();
    CState->StateHash = ComputeStateHash();
}

TSharedPtr<FRealmState> UCircuitsKernel::GetState() const
{
    return CState;
}

void UCircuitsKernel::SetState(const FRealmState& NewState)
{
    const FCircuitsState* Casted = static_cast<const FCircuitsState*>(&NewState);
    if (!Casted) return;

    CState = MakeShared<FCircuitsState>(*Casted);
    SimTime = CState->SimulationTime;
    StepCount = CState->StepCount;
}

void UCircuitsKernel::SerializeState(FArchive& Ar)
{
    if (CState.IsValid())
    {
        Ar << *CState;
    }
}

void UCircuitsKernel::DeserializeState(FArchive& Ar)
{
    FCircuitsState Loaded;
    Ar << Loaded;
    SetState(Loaded);
}

uint64 UCircuitsKernel::ComputeStateHash() const
{
    uint64 Hash = 0x1F83D9ABFB41BD6BULL;
    for (const FResonantTank& Tank : CState->Tanks)
    {
        Hash ^= ManifoldHashDoubleBits(Tank.Frequency) * 0x9E3779B97F4A7C15ULL;
        Hash ^= ManifoldHashDoubleBits(Tank.Phase) * 0xBF58476D1CE4E5B9ULL;
    }
    return Hash;
}

bool UCircuitsKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("CircuitResonance"))
    {
        const float MinStr = QueryObj.MinStrength;
        const FCircuitRatioMatch* Best = nullptr;
        for (const FCircuitRatioMatch& Match : CState->ActiveRatios)
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
            Result.StructureId = FGuid(GetTypeHash(GetRealmId().ToString()),
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

void UCircuitsKernel::QueryAll(const FRealmQuery& QueryDesc, TArray<FRealmQueryResult>& OutResults) const
{
    if (QueryDesc.QueryType != TEXT("CircuitResonance")) return;

    const float MinStr = QueryDesc.MinStrength;
    for (const FCircuitRatioMatch& Match : CState->ActiveRatios)
    {
        if (Match.Strength < MinStr) continue;

        FRealmQueryResult Result;
        Result.StructureId = FGuid(GetTypeHash(GetRealmId().ToString()),
            static_cast<uint32>(Match.Ratio.X), static_cast<uint32>(Match.Ratio.Y),
            GetSimulationVersion());
        Result.StructureType = QueryDesc.QueryType;
        Result.Strength = static_cast<float>(Match.Strength);
        Result.Parameters.Add(TEXT("Ratio"), FString::Printf(TEXT("%d:%d"), Match.Ratio.X, Match.Ratio.Y));
        OutResults.Add(Result);
    }
}

TArray<FName> UCircuitsKernel::GetSupportedQueryTypes() const
{
    return { TEXT("CircuitResonance") };
}

TMap<FName, FString> UCircuitsKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("TankCount"), TEXT("int"));
    return Schema;
}

bool UCircuitsKernel::SetParameter(FName /*Name*/, const FString& /*Value*/)
{
    return false;
}

bool UCircuitsKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("TankCount"))
    {
        OutValue = FString::FromInt(CState->Tanks.Num());
        return true;
    }
    return false;
}

bool UCircuitsKernel::VerifyDeterminism(int32 NumSteps) const
{
    UCircuitsKernel* SimA = NewObject<UCircuitsKernel>();
    UCircuitsKernel* SimB = NewObject<UCircuitsKernel>();
    SimA->Initialize(CState->Seed);
    SimB->Initialize(CState->Seed);

    for (const FResonantTank& Tank : CState->Tanks)
    {
        SimA->AddTank(Tank.Frequency, Tank.Amplitude);
        SimB->AddTank(Tank.Frequency, Tank.Amplitude);
    }

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.01f);
        SimB->Step(0.01f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

void UCircuitsKernel::AddTank(double Frequency, double Amplitude)
{
    FResonantTank Tank;
    Tank.Frequency = FMath::Max(0.0, Frequency);
    Tank.Amplitude = Amplitude;
    Tank.Phase = 0.0;
    CState->Tanks.Add(Tank);

    UpdateDerivedState();
}

const TArray<FCircuitRatioMatch>& UCircuitsKernel::GetActiveRatios() const
{
    return CState->ActiveRatios;
}

void UCircuitsKernel::DetectCircuitRatios()
{
    CState->ActiveRatios.Empty();

    const TArray<FResonantTank>& Tanks = CState->Tanks;
    for (int32 i = 0; i < Tanks.Num(); ++i)
    {
        for (int32 j = i + 1; j < Tanks.Num(); ++j)
        {
            const double Fa = Tanks[i].Frequency;
            const double Fb = Tanks[j].Frequency;
            if (Fa <= 0.0 || Fb <= 0.0) continue;

            const double Hi = FMath::Max(Fa, Fb);
            const double Lo = FMath::Min(Fa, Fb);

            // Exact integer ratio when both frequencies are (near) whole numbers — the
            // resonant-frequency counterpart of the exact tooth-count ratio in Gears.
            const int32 HiI = FMath::RoundToInt(Hi);
            const int32 LoI = FMath::RoundToInt(Lo);
            if (HiI > 0 && LoI > 0
                && FMath::Abs(Hi - HiI) < 1.0e-6 && FMath::Abs(Lo - LoI) < 1.0e-6)
            {
                const int32 G = FMath::GreatestCommonDivisor(HiI, LoI);
                if (G <= 0) continue;

                FCircuitRatioMatch Match;
                Match.Ratio = FIntPoint(HiI / G, LoI / G); // exact, reduced
                Match.Strength = 1.0;
                CState->ActiveRatios.Add(Match);
            }
        }
    }
}
