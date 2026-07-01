// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "OrbitsKernel.h"
#include "UObject/UObjectGlobals.h"

UOrbitsKernel::UOrbitsKernel()
{
    OrbitState = MakeShared<FOrbitsState>();
}

void UOrbitsKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    OrbitState = MakeShared<FOrbitsState>();
    OrbitState->RealmId = GetRealmId();
    OrbitState->SimulationVersion = GetSimulationVersion();
    OrbitState->Seed = Seed;
    OrbitState->SimulationTime = 0.0;
    OrbitState->StepCount = 0;

    Positions.Empty();
    Velocities.Empty();
    Accelerations.Empty();
    Masses.Empty();
    BodyIds.Empty();
}

void UOrbitsKernel::Reset()
{
    Initialize(OrbitState->Seed);
}

void UOrbitsKernel::Shutdown()
{
    OrbitState.Reset();
    Positions.Empty();
    Velocities.Empty();
    Accelerations.Empty();
    Masses.Empty();
    BodyIds.Empty();
}

void UOrbitsKernel::Step(float DeltaTime)
{
    if (BodyIds.Num() == 0) return;

    double dt = static_cast<double>(DeltaTime);
    double SubStepDt = dt / MaxSubSteps;

    // Sub-stepped Verlet integration
    for (int32 SubStep = 0; SubStep < MaxSubSteps; ++SubStep)
    {
        IntegrateVelocityVerlet(SubStepDt);
    }

    SimTime += dt;
    StepCount++;

    // Update state object
    OrbitState->SimulationTime = SimTime;
    OrbitState->StepCount = StepCount;

    // Sync state positions & velocities
    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        OrbitState->Bodies[i].Position = Positions[i];
        OrbitState->Bodies[i].Velocity = Velocities[i];
    }

    UpdateDerivedState();
}

void UOrbitsKernel::UpdateDerivedState()
{
    // Recompute orbital elements + resonances for all bodies
    RecomputeOrbitalElements();
    DetectResonances();

    // Compute system totals (energy is the baseline the energy-conservation test reads)
    double TotalEnergy = 0.0;
    FVector TotalAngularMomentum = FVector::ZeroVector;

    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        // Kinetic Energy = 0.5 * m * v^2
        double KE = 0.5 * Masses[i] * Velocities[i].SizeSquared();
        TotalEnergy += KE;

        // Potential Energy
        for (int32 j = i + 1; j < BodyIds.Num(); ++j)
        {
            double Dist = FVector::Dist(Positions[i], Positions[j]);
            TotalEnergy -= (G * Masses[i] * Masses[j]) / FMath::Max(Dist, Softening);
        }

        // Angular momentum: L = m * (r x v)
        TotalAngularMomentum += Masses[i] * FVector::CrossProduct(Positions[i], Velocities[i]);
    }

    OrbitState->SystemTotalEnergy = TotalEnergy;
    OrbitState->SystemTotalAngularMomentum = TotalAngularMomentum.Size();
    OrbitState->StateHash = ComputeStateHash();
}

void UOrbitsKernel::StepMultiple(float DeltaTime, int32 NumSteps)
{
    for (int32 i = 0; i < NumSteps; ++i)
    {
        Step(DeltaTime);
    }
}

TSharedPtr<FRealmState> UOrbitsKernel::GetState() const
{
    return OrbitState;
}

void UOrbitsKernel::SetState(const FRealmState& NewState)
{
    const FOrbitsState* CastedState = static_cast<const FOrbitsState*>(&NewState);
    if (!CastedState) return;

    OrbitState = MakeShared<FOrbitsState>(*CastedState);
    SimTime = OrbitState->SimulationTime;
    StepCount = OrbitState->StepCount;

    // Sync integration arrays
    Positions.Empty();
    Velocities.Empty();
    Accelerations.Empty();
    Masses.Empty();
    BodyIds.Empty();

    for (const FOrbitsBodyDef& Body : OrbitState->Bodies)
    {
        Positions.Add(Body.Position);
        Velocities.Add(Body.Velocity);
        Accelerations.Add(FVector::ZeroVector);
        Masses.Add(Body.Mass);
        BodyIds.Add(Body.Id);
    }

    ComputeAccelerations();
}

void UOrbitsKernel::SerializeState(FArchive& Ar)
{
    if (OrbitState.IsValid())
    {
        Ar << *OrbitState;
    }
}

void UOrbitsKernel::DeserializeState(FArchive& Ar)
{
    FOrbitsState LoadedState;
    Ar << LoadedState;
    SetState(LoadedState);
}

uint64 UOrbitsKernel::ComputeStateHash() const
{
    // Compute simple deterministic bitwise hash of positions and velocities
    uint64 Hash = 0x123456789ABCDEF0ULL;
    for (int32 i = 0; i < Positions.Num(); ++i)
    {
        auto HashDouble = [](double Val) -> uint64 {
            uint64 Bits;
            FMemory::Memcpy(&Bits, &Val, sizeof(double));
            return Bits;
        };

        Hash ^= HashDouble(Positions[i].X) * 0x9E3779B97F4A7C15ULL;
        Hash ^= HashDouble(Positions[i].Y) * 0xBF58476D1CE4E5B9ULL;
        Hash ^= HashDouble(Positions[i].Z) * 0x123ULL;

        Hash ^= HashDouble(Velocities[i].X) * 0x9E3779B97F4A7C15ULL;
        Hash ^= HashDouble(Velocities[i].Y) * 0xBF58476D1CE4E5B9ULL;
        Hash ^= HashDouble(Velocities[i].Z) * 0x456ULL;
    }
    return Hash;
}

bool UOrbitsKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("OrbitalResonance"))
    {
        // Return the STRONGEST qualifying resonance inside bounds (matching the
        // strongest-match semantics of every other realm's query).
        const double MinStr = QueryObj.MinStrength;
        const FResonanceMatch* Best = nullptr;
        const FOrbitsBodyDef* BestBodyA = nullptr;

        for (const FResonanceMatch& Res : OrbitState->ActiveResonances)
        {
            if (Res.Strength < MinStr) continue;

            const FOrbitsBodyDef* BodyA = GetBody(Res.BodyA);
            if (BodyA && QueryObj.SearchBounds.IsValid && !QueryObj.SearchBounds.IsInside(BodyA->Position))
            {
                continue;
            }
            if (!Best || Res.Strength > Best->Strength)
            {
                Best = &Res;
                BestBodyA = BodyA;
            }
        }

        if (Best)
        {
            Result.StructureId = Best->Id; // stable across frames (see DetectResonances)
            Result.StructureType = QueryObj.QueryType;
            Result.Position = BestBodyA ? BestBodyA->Position : FVector::ZeroVector;
            Result.Rotation = FQuat::Identity;
            Result.Strength = static_cast<float>(Best->Strength);
            Result.Parameters.Add(TEXT("Ratio"), FString::Printf(TEXT("%d:%d"), Best->Ratio.X, Best->Ratio.Y));
            Result.Parameters.Add(TEXT("Deviation"), FString::Printf(TEXT("%f"), Best->Deviation));
            Result.Parameters.Add(TEXT("BodyA"), Best->BodyA.ToString());
            Result.Parameters.Add(TEXT("BodyB"), Best->BodyB.ToString());
            return true;
        }
    }
    return false;
}

TArray<FName> UOrbitsKernel::GetSupportedQueryTypes() const
{
    return { TEXT("OrbitalResonance") };
}

TMap<FName, FString> UOrbitsKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("G"), TEXT("float"));
    Schema.Add(TEXT("Softening"), TEXT("float"));
    Schema.Add(TEXT("bFullNBody"), TEXT("bool"));
    return Schema;
}

bool UOrbitsKernel::SetParameter(FName Name, const FString& Value)
{
    if (Name == TEXT("G"))
    {
        G = FCString::Atod(*Value);
        return true;
    }
    else if (Name == TEXT("Softening"))
    {
        Softening = FCString::Atod(*Value);
        return true;
    }
    else if (Name == TEXT("bFullNBody"))
    {
        bFullNBody = Value.ToBool();
        return true;
    }
    return false;
}

bool UOrbitsKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("G"))
    {
        OutValue = FString::Printf(TEXT("%e"), G);
        return true;
    }
    else if (Name == TEXT("Softening"))
    {
        OutValue = FString::Printf(TEXT("%f"), Softening);
        return true;
    }
    else if (Name == TEXT("bFullNBody"))
    {
        OutValue = bFullNBody ? TEXT("true") : TEXT("false");
        return true;
    }
    return false;
}

bool UOrbitsKernel::VerifyDeterminism(int32 NumSteps) const
{
    // Two fresh sims from the same seed + bodies, advanced identically, must produce
    // bitwise-identical state. Comparing against `this` would be wrong: `this` may be
    // at a different step count than the freshly-advanced sim (the same fix the Fluids
    // and Waves kernels already use).
    UOrbitsKernel* SimA = NewObject<UOrbitsKernel>();
    UOrbitsKernel* SimB = NewObject<UOrbitsKernel>();
    SimA->Initialize(OrbitState->Seed);
    SimB->Initialize(OrbitState->Seed);

    for (const FOrbitsBodyDef& Body : OrbitState->Bodies)
    {
        SimA->AddBody(Body);
        SimB->AddBody(Body);
    }

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.016666f);
        SimB->Step(0.016666f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

FGuid UOrbitsKernel::AddBody(const FOrbitsBodyDef& Def)
{
    FOrbitsBodyDef NewDef = Def;
    if (!NewDef.Id.IsValid())
    {
        NewDef.Id = FGuid::NewGuid();
    }

    OrbitState->Bodies.Add(NewDef);
    Positions.Add(NewDef.Position);
    Velocities.Add(NewDef.Velocity);
    Accelerations.Add(FVector::ZeroVector);
    Masses.Add(NewDef.Mass);
    BodyIds.Add(NewDef.Id);

    ComputeAccelerations();
    // Compute initial elements/resonances/energy so the pre-Step baseline is valid.
    UpdateDerivedState();
    return NewDef.Id;
}

bool UOrbitsKernel::RemoveBody(FGuid BodyId)
{
    int32 Index = BodyIds.Find(BodyId);
    if (Index == INDEX_NONE) return false;

    OrbitState->Bodies.RemoveAt(Index);
    Positions.RemoveAt(Index);
    Velocities.RemoveAt(Index);
    Accelerations.RemoveAt(Index);
    Masses.RemoveAt(Index);
    BodyIds.RemoveAt(Index);

    ComputeAccelerations();
    return true;
}

const FOrbitsBodyDef* UOrbitsKernel::GetBody(FGuid BodyId) const
{
    for (const FOrbitsBodyDef& Body : OrbitState->Bodies)
    {
        if (Body.Id == BodyId) return &Body;
    }
    return nullptr;
}

bool UOrbitsKernel::GetOrbitalElements(FGuid BodyId, FOrbitalElements& OutElements) const
{
    if (OrbitState->CurrentElements.Contains(BodyId))
    {
        OutElements = OrbitState->CurrentElements[BodyId];
        return true;
    }
    return false;
}

const TArray<FResonanceMatch>& UOrbitsKernel::GetActiveResonances() const
{
    return OrbitState->ActiveResonances;
}

void UOrbitsKernel::DetectResonances(double MaxDeviation, int32 MaxRatio)
{
    OrbitState->ActiveResonances.Empty();

    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        if (OrbitState->Bodies[i].bIsCentral) continue;

        FOrbitalElements ElemA;
        if (!GetOrbitalElements(BodyIds[i], ElemA) || !ElemA.IsValid()) continue;

        for (int32 j = i + 1; j < BodyIds.Num(); ++j)
        {
            if (OrbitState->Bodies[j].bIsCentral) continue;

            FOrbitalElements ElemB;
            if (!GetOrbitalElements(BodyIds[j], ElemB) || !ElemB.IsValid()) continue;

            double PeriodA = ElemA.Period;
            double PeriodB = ElemB.Period;
            if (PeriodA <= 0.0 || PeriodB <= 0.0) continue;

            // Express the resonance as the outer:inner period ratio (>= 1) so a
            // period ratio of 1.5 is labelled 3:2 (not 2:3). p >= q by construction.
            double RatioVal = FMath::Max(PeriodA, PeriodB) / FMath::Min(PeriodA, PeriodB);

            // Search for rational p/q
            int32 BestP = 1;
            int32 BestQ = 1;
            double BestDev = 1e9;

            for (int32 p = 1; p <= MaxRatio; ++p)
            {
                for (int32 q = 1; q <= MaxRatio; ++q)
                {
                    double Rational = static_cast<double>(p) / q;
                    double Dev = FMath::Abs(RatioVal - Rational);
                    if (Dev < BestDev)
                    {
                        BestDev = Dev;
                        BestP = p;
                        BestQ = q;
                    }
                }
            }

            if (BestDev <= MaxDeviation)
            {
                FResonanceMatch Match;
                Match.BodyA = BodyIds[i];
                Match.BodyB = BodyIds[j];
                Match.Ratio = FIntPoint(BestP, BestQ);
                Match.ActualRatio = RatioVal;
                Match.Deviation = BestDev;
                // Strength falls off exponentially with deviation
                Match.Strength = FMath::Exp(-200.0 * BestDev);
                // Stable identity from the (unordered) body pair — same pair, same id.
                Match.Id = FGuid(
                    BodyIds[i].A ^ BodyIds[j].A,
                    BodyIds[i].B ^ BodyIds[j].B,
                    BodyIds[i].C ^ BodyIds[j].C,
                    BodyIds[i].D ^ BodyIds[j].D);
                OrbitState->ActiveResonances.Add(Match);
            }
        }
    }
}

void UOrbitsKernel::RecomputeOrbitalElements()
{
    OrbitState->CurrentElements.Empty();

    // Find central body
    int32 CentralIndex = INDEX_NONE;
    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        if (OrbitState->Bodies[i].bIsCentral)
        {
            CentralIndex = i;
            break;
        }
    }

    if (CentralIndex == INDEX_NONE) return;

    double Mc = Masses[CentralIndex];
    FVector Rc = Positions[CentralIndex];
    FVector Vc = Velocities[CentralIndex];

    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        if (i == CentralIndex) continue;

        double Mp = Masses[i];
        FVector R = Positions[i] - Rc;
        FVector V = Velocities[i] - Vc;

        double r = R.Size();
        double vSqr = V.SizeSquared();

        if (r <= 0.0) continue;

        double Mu = G * (Mc + Mp);

        // Specific angular momentum: h = r x v
        FVector hVec = FVector::CrossProduct(R, V);
        double h = hVec.Size();

        // Node vector: n = k x h
        FVector nVec(-hVec.Y, hVec.X, 0.0);
        double n = nVec.Size();

        // Specific energy: epsilon = v^2 / 2 - mu / r
        double Energy = (vSqr * 0.5) - (Mu / r);

        // Semi-major axis: a = -mu / 2*epsilon
        double a = 0.0;
        if (Energy < 0.0)
        {
            a = -Mu / (2.0 * Energy);
        }

        // Eccentricity vector
        FVector eVec = ((vSqr - Mu / r) * R - FVector::DotProduct(R, V) * V) / Mu;
        double e = eVec.Size();

        // Inclination
        double Inclination = (h > 0.0) ? FMath::Acos(hVec.Z / h) : 0.0;

        // Longitude of ascending node (Omega)
        double Omega = 0.0;
        if (n > 0.0)
        {
            Omega = FMath::Acos(nVec.X / n);
            if (nVec.Y < 0.0) Omega = (2.0 * PI) - Omega;
        }

        // Argument of periapsis (omega)
        double ArgPeri = 0.0;
        if (e > 0.0)
        {
            if (n > 0.0)
            {
                ArgPeri = FMath::Acos(FVector::DotProduct(nVec, eVec) / (n * e));
                if (eVec.Z < 0.0) ArgPeri = (2.0 * PI) - ArgPeri;
            }
            else
            {
                // Equatorial orbit: longitude of periapsis
                ArgPeri = FMath::Atan2(eVec.Y, eVec.X);
                if (ArgPeri < 0.0) ArgPeri += (2.0 * PI);
            }
        }

        // True anomaly (nu)
        double Nu = 0.0;
        if (r > 0.0)
        {
            if (e > 1e-6)
            {
                double CosNu = FVector::DotProduct(eVec, R) / (e * r);
                CosNu = FMath::Clamp(CosNu, -1.0, 1.0);
                Nu = FMath::Acos(CosNu);
                if (FVector::DotProduct(R, V) < 0.0) Nu = (2.0 * PI) - Nu;
            }
            else
            {
                // Circular
                if (n > 0.0)
                {
                    double CosNu = FVector::DotProduct(nVec, R) / (n * r);
                    CosNu = FMath::Clamp(CosNu, -1.0, 1.0);
                    Nu = FMath::Acos(CosNu);
                    if (V.Z < 0.0) Nu = (2.0 * PI) - Nu;
                }
                else
                {
                    Nu = FMath::Atan2(R.Y, R.X);
                    if (Nu < 0.0) Nu += (2.0 * PI);
                }
            }
        }

        // Period
        double Period = 0.0;
        double MeanMotion = 0.0;
        if (a > 0.0)
        {
            Period = 2.0 * PI * FMath::Sqrt((a * a * a) / Mu);
            MeanMotion = 2.0 * PI / Period;
        }

        FOrbitalElements Elem;
        Elem.SemiMajorAxis = a;
        Elem.Eccentricity = e;
        Elem.Inclination = Inclination;
        Elem.LongitudeOfAscendingNode = Omega;
        Elem.ArgumentOfPeriapsis = ArgPeri;
        Elem.TrueAnomaly = Nu;
        Elem.Period = Period;
        Elem.MeanMotion = MeanMotion;

        OrbitState->CurrentElements.Add(BodyIds[i], Elem);
    }
}

void UOrbitsKernel::ComputeAccelerations()
{
    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        Accelerations[i] = FVector::ZeroVector;
    }

    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        for (int32 j = i + 1; j < BodyIds.Num(); ++j)
        {
            FVector Dir = Positions[j] - Positions[i];
            double DistSqr = Dir.SizeSquared();
            double Dist = FMath::Sqrt(DistSqr);
            if (Dist <= 0.0) continue;

            // F = G * m1 * m2 / (r^2 + Softening^2)
            // a1 = F / m1 = G * m2 * Dir / (r * (r^2 + Softening^2))
            double SoftenedDistSqr = DistSqr + Softening * Softening;
            double SoftenedDistCube = SoftenedDistSqr * FMath::Sqrt(SoftenedDistSqr);

            FVector ForceDir = Dir / Dist;

            if (bFullNBody)
            {
                // Full gravitational interaction
                Accelerations[i] += (G * Masses[j] * Dir) / SoftenedDistCube;
                Accelerations[j] -= (G * Masses[i] * Dir) / SoftenedDistCube;
            }
            else
            {
                // Simple attraction to central star only
                if (OrbitState->Bodies[i].bIsCentral)
                {
                    Accelerations[j] -= (G * Masses[i] * Dir) / SoftenedDistCube;
                }
                else if (OrbitState->Bodies[j].bIsCentral)
                {
                    Accelerations[i] += (G * Masses[j] * Dir) / SoftenedDistCube;
                }
            }
        }
    }
}

void UOrbitsKernel::IntegrateVelocityVerlet(double DeltaTime)
{
    // 1. Position update: x(t + dt) = x(t) + v(t)*dt + 0.5*a(t)*dt^2
    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        Positions[i] += Velocities[i] * DeltaTime + 0.5 * Accelerations[i] * DeltaTime * DeltaTime;
    }

    // Store old accelerations
    TArray<FVector> OldAccelerations = Accelerations;

    // 2. Recompute accelerations based on new positions
    ComputeAccelerations();

    // 3. Velocity update: v(t + dt) = v(t) + 0.5*(a(t) + a(t + dt))*dt
    for (int32 i = 0; i < BodyIds.Num(); ++i)
    {
        Velocities[i] += 0.5 * (OldAccelerations[i] + Accelerations[i]) * DeltaTime;
    }
}

double UOrbitsKernel::ComputeResonanceStrength(const FOrbitalElements& A, const FOrbitalElements& B, int32 p, int32 q) const
{
    // Unused helper matching signature - we compute strength exponentially based on deviation in DetectResonances
    return FMath::Exp(-200.0 * FMath::Abs((A.Period / B.Period) - (static_cast<double>(p) / q)));
}
