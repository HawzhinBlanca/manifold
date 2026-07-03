// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "FluidsKernel.h"
#include "UObject/UObjectGlobals.h"

UFluidsKernel::UFluidsKernel()
{
    FluidsState = MakeShared<FFluidsState>();
}

void UFluidsKernel::Initialize(uint64 Seed)
{
    SimTime = 0.0;
    StepCount = 0;
    RNG.Initialize(Seed);

    FluidsState = MakeShared<FFluidsState>();
    FluidsState->RealmId = GetRealmId();
    FluidsState->SimulationVersion = GetSimulationVersion();
    FluidsState->Seed = Seed;
    FluidsState->SimulationTime = 0.0;
    FluidsState->StepCount = 0;
    FluidsState->GridSize = Size;

    int32 ArraySize = (Size + 2) * (Size + 2);
    u.Init(0.0f, ArraySize);
    v.Init(0.0f, ArraySize);
    u_prev.Init(0.0f, ArraySize);
    v_prev.Init(0.0f, ArraySize);
    d.Init(0.0f, ArraySize);
    d_prev.Init(0.0f, ArraySize);

    // Initial state: deterministic perturbations based on seed
    FDeterministicRNG TempRNG(Seed);
    for (int32 j = 1; j <= Size; ++j)
    {
        for (int32 i = 1; i <= Size; ++i)
        {
            if (TempRNG.FRand() < 0.1f)
            {
                // Add some initial velocity & density
                int32 idx = IX(i, j);
                u[idx] = TempRNG.FRandRange(-2.0f, 2.0f);
                v[idx] = TempRNG.FRandRange(-2.0f, 2.0f);
                d[idx] = TempRNG.FRandRange(0.1f, 1.0f);
            }
        }
    }
}

void UFluidsKernel::Reset()
{
    Initialize(FluidsState->Seed);
}

void UFluidsKernel::Shutdown()
{
    FluidsState.Reset();
    u.Empty();
    v.Empty();
    u_prev.Empty();
    v_prev.Empty();
    d.Empty();
    d_prev.Empty();
}

void UFluidsKernel::Step(float DeltaTime)
{
    TickFluid(DeltaTime);

    SimTime += DeltaTime;
    StepCount++;

    FluidsState->SimulationTime = SimTime;
    FluidsState->StepCount = StepCount;

    // Sync state
    FluidsState->Density = d;
    FluidsState->VelocityX = u;
    FluidsState->VelocityY = v;

    DetectVortices();
    FluidsState->StateHash = ComputeStateHash();
}

TSharedPtr<FRealmState> UFluidsKernel::GetState() const
{
    return FluidsState;
}

void UFluidsKernel::SetState(const FRealmState& NewState)
{
    const FFluidsState* CastedState = static_cast<const FFluidsState*>(&NewState);
    if (!CastedState) return;

    // GridSize and the three grid arrays (Density/VelocityX/VelocityY) are serialized
    // independently, so a corrupted or hostile snapshot can carry a GridSize that is
    // inconsistent with the array lengths, or a huge GridSize whose (Size+2)^2 overflows int32.
    // Either would drive out-of-bounds indexing on the very next Step (IX runs to (Size+2)^2).
    // Treat the incoming state as UNTRUSTED: adopt its grids only when GridSize is in range AND
    // all three arrays are exactly (GridSize+2)^2 long; otherwise fall back to a clean zeroed
    // field at the clamped size so the kernel stays valid and deterministic. (Legit self-consistent
    // states — the round-trip path — clamp to themselves and are copied verbatim, unchanged.)
    const int32 ClampedSize = FMath::Clamp(CastedState->GridSize, 1, MaxGridSize);
    const int64 ExpectedLen = static_cast<int64>(ClampedSize + 2) * static_cast<int64>(ClampedSize + 2);
    const bool bConsistent =
        CastedState->GridSize == ClampedSize &&
        static_cast<int64>(CastedState->Density.Num())   == ExpectedLen &&
        static_cast<int64>(CastedState->VelocityX.Num()) == ExpectedLen &&
        static_cast<int64>(CastedState->VelocityY.Num()) == ExpectedLen;

    FluidsState = MakeShared<FFluidsState>(*CastedState);
    SimTime = FluidsState->SimulationTime;
    StepCount = FluidsState->StepCount;
    Size = ClampedSize;

    const int32 ArraySize = static_cast<int32>(ExpectedLen); // safe: ClampedSize <= MaxGridSize
    if (bConsistent)
    {
        d = FluidsState->Density;
        u = FluidsState->VelocityX;
        v = FluidsState->VelocityY;
    }
    else
    {
        // Malformed/hostile state: reset to a safe zeroed field and repair the stored state so
        // GetState / re-serialization stays self-consistent.
        d.Init(0.0f, ArraySize);
        u.Init(0.0f, ArraySize);
        v.Init(0.0f, ArraySize);
        FluidsState->GridSize = ClampedSize;
        FluidsState->Density = d;
        FluidsState->VelocityX = u;
        FluidsState->VelocityY = v;
    }
    u_prev.Init(0.0f, ArraySize);
    v_prev.Init(0.0f, ArraySize);
    d_prev.Init(0.0f, ArraySize);
}

void UFluidsKernel::SerializeState(FArchive& Ar)
{
    if (FluidsState.IsValid())
    {
        Ar << *FluidsState;
    }
}

void UFluidsKernel::DeserializeState(FArchive& Ar)
{
    FFluidsState LoadedState;
    Ar << LoadedState;
    SetState(LoadedState);
}

uint64 UFluidsKernel::ComputeStateHash() const
{
    // Bitwise hash of ALL simulation-driving grids: density AND both velocity
    // components. The velocity fields u/v drive advection (Advect) and vortex
    // detection (DetectVortices) every step, so a state that diverges only in
    // velocity — with byte-identical density — MUST hash differently, or the
    // divergence goes UNDETECTED (this hash IS the determinism/divergence detector
    // compared by VerifyDeterminism and the replay layer). Distinct per-grid value
    // and index multipliers keep the folds from cancelling across equal-length grids.
    uint64 Hash = 0xFEDCBA9876543210ULL;
    auto FoldGrid = [&Hash](const TArray<float>& Grid, uint64 ValMul, uint64 IdxMul)
    {
        for (int32 i = 0; i < Grid.Num(); ++i)
        {
            uint32 Bits;
            float Val = Grid[i];
            FMemory::Memcpy(&Bits, &Val, sizeof(float));
            Hash ^= static_cast<uint64>(Bits) * ValMul;
            Hash ^= static_cast<uint64>(i) * IdxMul;
        }
    };
    FoldGrid(d, 0x9E3779B97F4A7C15ULL, 0xBF58476D1CE4E5B9ULL); // density (unchanged constants)
    FoldGrid(u, 0xC2B2AE3D27D4EB4FULL, 0x165667B19E3779F9ULL); // velocity X
    FoldGrid(v, 0xD6E8FEB86659FD93ULL, 0xCB92BA72B4C7E9FBULL); // velocity Y
    return Hash;
}

bool UFluidsKernel::Query(const FRealmQuery& QueryObj, FRealmQueryResult& Result) const
{
    if (QueryObj.QueryType == TEXT("VortexCenter"))
    {
        const float MinStr = QueryObj.MinStrength;

        // Return the STRONGEST qualifying vortex, not the first in scan order.
        // The grid is seeded with random noise, so scan order would often return a
        // weak noise vortex; the dominant (injected) vortex is what a "VortexCenter"
        // query means.
        const FFluidVortex* Best = nullptr;
        for (const FFluidVortex& Vor : FluidsState->ActiveVortices)
        {
            if (FMath::Abs(Vor.Strength) < MinStr) continue;
            if (QueryObj.SearchBounds.IsValid && !QueryObj.SearchBounds.IsInside(Vor.Position)) continue;
            if (!Best || FMath::Abs(Vor.Strength) > FMath::Abs(Best->Strength))
            {
                Best = &Vor;
            }
        }

        if (Best)
        {
            Result.StructureId = Best->Id;
            Result.StructureType = QueryObj.QueryType;
            Result.Position = Best->Position;
            Result.Rotation = FQuat::Identity;
            Result.Strength = FMath::Abs(Best->Strength);
            Result.Parameters.Add(TEXT("Strength"), FString::Printf(TEXT("%f"), Best->Strength));
            Result.Parameters.Add(TEXT("Radius"), FString::Printf(TEXT("%f"), Best->Radius));
            return true;
        }
    }
    return false;
}

TArray<FName> UFluidsKernel::GetSupportedQueryTypes() const
{
    return { TEXT("VortexCenter") };
}

TMap<FName, FString> UFluidsKernel::GetParameterSchema() const
{
    TMap<FName, FString> Schema;
    Schema.Add(TEXT("Viscosity"), TEXT("float"));
    Schema.Add(TEXT("Diffusion"), TEXT("float"));
    Schema.Add(TEXT("Decay"), TEXT("float"));
    return Schema;
}

bool UFluidsKernel::SetParameter(FName Name, const FString& Value)
{
    if (Name == TEXT("Viscosity"))
    {
        // Diffusion coefficients are physical and must be >= 0. A negative value drives the
        // implicit-solve denominator (1 + 4a) in Diffuse to zero/negative -> Inf/NaN that
        // propagates into Advect and casts to an out-of-bounds array index. Clamp, don't trust.
        Viscosity = FMath::Max(0.0f, FCString::Atof(*Value));
        return true;
    }
    else if (Name == TEXT("Diffusion"))
    {
        Diffusion = FMath::Max(0.0f, FCString::Atof(*Value));
        return true;
    }
    else if (Name == TEXT("Decay"))
    {
        Decay = FCString::Atof(*Value);
        return true;
    }
    return false;
}

bool UFluidsKernel::GetParameter(FName Name, FString& OutValue) const
{
    if (Name == TEXT("Viscosity"))
    {
        OutValue = FString::Printf(TEXT("%f"), Viscosity);
        return true;
    }
    else if (Name == TEXT("Diffusion"))
    {
        OutValue = FString::Printf(TEXT("%f"), Diffusion);
        return true;
    }
    else if (Name == TEXT("Decay"))
    {
        OutValue = FString::Printf(TEXT("%f"), Decay);
        return true;
    }
    return false;
}

bool UFluidsKernel::VerifyDeterminism(int32 NumSteps) const
{
    // Determinism means: two fresh sims from the same seed, advanced identically,
    // produce bitwise-identical state. (Comparing against `this` would be wrong —
    // `this` may be at a different step count than the freshly-advanced sim.)
    UFluidsKernel* SimA = NewObject<UFluidsKernel>();
    SimA->Initialize(FluidsState->Seed);
    UFluidsKernel* SimB = NewObject<UFluidsKernel>();
    SimB->Initialize(FluidsState->Seed);

    for (int32 i = 0; i < NumSteps; ++i)
    {
        SimA->Step(0.016666f);
        SimB->Step(0.016666f);
    }

    return SimA->ComputeStateHash() == SimB->ComputeStateHash();
}

void UFluidsKernel::AddDensity(float X, float Y, float Amount)
{
    int32 i = static_cast<int32>(X * Size) + 1;
    int32 j = static_cast<int32>(Y * Size) + 1;
    if (i >= 1 && i <= Size && j >= 1 && j <= Size)
    {
        d[IX(i, j)] += Amount;
    }
}

void UFluidsKernel::AddVelocity(float X, float Y, float AmountX, float AmountY)
{
    int32 i = static_cast<int32>(X * Size) + 1;
    int32 j = static_cast<int32>(Y * Size) + 1;
    if (i >= 1 && i <= Size && j >= 1 && j <= Size)
    {
        u[IX(i, j)] += AmountX;
        v[IX(i, j)] += AmountY;
    }
}

float UFluidsKernel::GetDensityAt(float X, float Y) const
{
    int32 i = static_cast<int32>(X * Size) + 1;
    int32 j = static_cast<int32>(Y * Size) + 1;
    if (i >= 1 && i <= Size && j >= 1 && j <= Size)
    {
        return d[IX(i, j)];
    }
    return 0.0f;
}

FVector UFluidsKernel::GetVelocityAt(float X, float Y) const
{
    int32 i = static_cast<int32>(X * Size) + 1;
    int32 j = static_cast<int32>(Y * Size) + 1;
    if (i >= 1 && i <= Size && j >= 1 && j <= Size)
    {
        return FVector(u[IX(i, j)], v[IX(i, j)], 0.0f);
    }
    return FVector::ZeroVector;
}

const TArray<FFluidVortex>& UFluidsKernel::GetActiveVortices() const
{
    return FluidsState->ActiveVortices;
}

// =====================================================================
// SOLVER IMPLEMENTATION
// =====================================================================

void UFluidsKernel::AddSource(TArray<float>& x, const TArray<float>& s, float dt)
{
    int32 SizeTotal = (Size + 2) * (Size + 2);
    for (int32 i = 0; i < SizeTotal; ++i)
    {
        x[i] += dt * s[i];
    }
}

void UFluidsKernel::Diffuse(int32 b, TArray<float>& x, const TArray<float>& x0, float diff, float dt)
{
    float a = dt * diff * Size * Size;
    for (int32 k = 0; k < 20; ++k)
    {
        for (int32 j = 1; j <= Size; ++j)
        {
            for (int32 i = 1; i <= Size; ++i)
            {
                x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i - 1, j)] + x[IX(i + 1, j)] + x[IX(i, j - 1)] + x[IX(i, j + 1)])) / (1.0f + 4.0f * a);
            }
        }
        SetBoundary(b, x);
    }
}

void UFluidsKernel::Advect(int32 b, TArray<float>& d_arr, const TArray<float>& d0, const TArray<float>& u_arr, const TArray<float>& v_arr, float dt)
{
    float dt0 = dt * Size;
    for (int32 j = 1; j <= Size; ++j)
    {
        for (int32 i = 1; i <= Size; ++i)
        {
            float x_coord = i - dt0 * u_arr[IX(i, j)];
            float y_coord = j - dt0 * v_arr[IX(i, j)];

            // A non-finite velocity would bypass the range clamps below (NaN compares false to
            // both bounds) and cast to an out-of-bounds int index. Snap any NaN/Inf back to the cell.
            if (!FMath::IsFinite(x_coord)) { x_coord = static_cast<float>(i); }
            if (!FMath::IsFinite(y_coord)) { y_coord = static_cast<float>(j); }

            if (x_coord < 0.5f) x_coord = 0.5f;
            if (x_coord > Size + 0.5f) x_coord = Size + 0.5f;
            int32 i0 = static_cast<int32>(x_coord);
            int32 i1 = i0 + 1;

            if (y_coord < 0.5f) y_coord = 0.5f;
            if (y_coord > Size + 0.5f) y_coord = Size + 0.5f;
            int32 j0 = static_cast<int32>(y_coord);
            int32 j1 = j0 + 1;

            float s1 = x_coord - i0;
            float s0 = 1.0f - s1;
            float t1 = y_coord - j0;
            float t0 = 1.0f - t1;

            d_arr[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)]) +
                              s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
        }
    }
    SetBoundary(b, d_arr);
}

void UFluidsKernel::Project(TArray<float>& u_arr, TArray<float>& v_arr, TArray<float>& p, TArray<float>& div)
{
    for (int32 j = 1; j <= Size; ++j)
    {
        for (int32 i = 1; i <= Size; ++i)
        {
            div[IX(i, j)] = -0.5f * (u_arr[IX(i + 1, j)] - u_arr[IX(i - 1, j)] + v_arr[IX(i, j + 1)] - v_arr[IX(i, j - 1)]) / Size;
            p[IX(i, j)] = 0.0f;
        }
    }
    SetBoundary(0, div);
    SetBoundary(0, p);

    for (int32 k = 0; k < 20; ++k)
    {
        for (int32 j = 1; j <= Size; ++j)
        {
            for (int32 i = 1; i <= Size; ++i)
            {
                p[IX(i, j)] = (div[IX(i, j)] + p[IX(i - 1, j)] + p[IX(i + 1, j)] + p[IX(i, j - 1)] + p[IX(i, j + 1)]) / 4.0f;
            }
        }
        SetBoundary(0, p);
    }

    for (int32 j = 1; j <= Size; ++j)
    {
        for (int32 i = 1; i <= Size; ++i)
        {
            u_arr[IX(i, j)] -= 0.5f * Size * (p[IX(i + 1, j)] - p[IX(i - 1, j)]);
            v_arr[IX(i, j)] -= 0.5f * Size * (p[IX(i, j + 1)] - p[IX(i, j - 1)]);
        }
    }
    SetBoundary(1, u_arr);
    SetBoundary(2, v_arr);
}

void UFluidsKernel::SetBoundary(int32 b, TArray<float>& x)
{
    for (int32 i = 1; i <= Size; ++i)
    {
        x[IX(0, i)] = b == 1 ? -x[IX(1, i)] : x[IX(1, i)];
        x[IX(Size + 1, i)] = b == 1 ? -x[IX(Size, i)] : x[IX(Size, i)];
        x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
        x[IX(i, Size + 1)] = b == 2 ? -x[IX(i, Size)] : x[IX(i, Size)];
    }

    x[IX(0, 0)] = 0.5f * (x[IX(1, 0)] + x[IX(0, 1)]);
    x[IX(0, Size + 1)] = 0.5f * (x[IX(1, Size + 1)] + x[IX(0, Size)]);
    x[IX(Size + 1, 0)] = 0.5f * (x[IX(Size, 0)] + x[IX(Size + 1, 1)]);
    x[IX(Size + 1, Size + 1)] = 0.5f * (x[IX(Size, Size + 1)] + x[IX(Size + 1, Size)]);
}

void UFluidsKernel::TickFluid(float dt)
{
    // 1. Solve velocity
    // u_prev/v_prev will hold current sources/forces
    AddSource(u, u_prev, dt);
    AddSource(v, v_prev, dt);

    Swap(u_prev, u);
    Diffuse(1, u, u_prev, Viscosity, dt);

    Swap(v_prev, v);
    Diffuse(2, v, v_prev, Viscosity, dt);

    Project(u, v, u_prev, v_prev);

    Swap(u_prev, u);
    Swap(v_prev, v);
    Advect(1, u, u_prev, u_prev, v_prev, dt);
    Advect(2, v, v_prev, u_prev, v_prev, dt);

    Project(u, v, u_prev, v_prev);

    // Reset velocity sources
    FMemory::Memzero(u_prev.GetData(), u_prev.Num() * sizeof(float));
    FMemory::Memzero(v_prev.GetData(), v_prev.Num() * sizeof(float));

    // 2. Solve density
    AddSource(d, d_prev, dt);

    Swap(d_prev, d);
    Diffuse(0, d, d_prev, Diffusion, dt);

    Swap(d_prev, d);
    Advect(0, d, d_prev, u, v, dt);

    // Apply decay and reset density sources
    FMemory::Memzero(d_prev.GetData(), d_prev.Num() * sizeof(float));
    for (int32 i = 0; i < d.Num(); ++i)
    {
        d[i] *= Decay;
    }
}

void UFluidsKernel::DetectVortices()
{
    TArray<FFluidVortex> NewVortices;

    // Detect vortex centers based on curl/vorticity local maxima
    for (int32 j = 2; j < Size; ++j)
    {
        for (int32 i = 2; i < Size; ++i)
        {
            // Curl = dv/dx - du/dy
            float curl = 0.5f * (v[IX(i + 1, j)] - v[IX(i - 1, j)]) - 0.5f * (u[IX(i, j + 1)] - u[IX(i, j - 1)]);

            // Check if absolute curl is a local maximum in a 3x3 neighborhood
            float abs_curl = FMath::Abs(curl);
            if (abs_curl > 0.05f)
            {
                bool bLocalMax = true;
                for (int32 dj = -1; dj <= 1; ++dj)
                {
                    for (int32 di = -1; di <= 1; ++di)
                    {
                        if (di == 0 && dj == 0) continue;
                        float n_curl = 0.5f * (v[IX(i + di + 1, j + dj)] - v[IX(i + di - 1, j + dj)]) - 
                                       0.5f * (u[IX(i + di, j + dj + 1)] - u[IX(i + di, j + dj - 1)]);
                        if (FMath::Abs(n_curl) > abs_curl)
                        {
                            bLocalMax = false;
                            break;
                        }
                    }
                    if (!bLocalMax) break;
                }

                if (bLocalMax)
                {
                    FFluidVortex Vortex;
                    float XNorm = static_cast<float>(i - 1) / Size;
                    float YNorm = static_cast<float>(j - 1) / Size;
                    Vortex.Position = FVector(XNorm * 1000.0f, YNorm * 1000.0f, 0.0f);
                    Vortex.Strength = curl;
                    Vortex.Radius = 50.0f; // Default vortex influence radius
                    NewVortices.Add(Vortex);
                }
            }
        }
    }

    // Match with previous active vortices to maintain GUID stability
    TArray<FFluidVortex> StableVortices;
    for (FFluidVortex& NewVor : NewVortices)
    {
        bool bMatched = false;
        for (const FFluidVortex& OldVor : FluidsState->ActiveVortices)
        {
            if (FVector::DistSquared(NewVor.Position, OldVor.Position) < 2500.0f) // 50 units distance threshold
            {
                NewVor.Id = OldVor.Id;
                bMatched = true;
                break;
            }
        }
        if (!bMatched)
        {
            // Deterministic identity from the realm + grid CELL, so the same vortex gets the same
            // id across identical runs (the stable-structure-id contract the sibling realms uphold).
            // Quantize by the actual cell spacing (1000/Size), NOT a fixed 50 units: cell spacing is
            // ~31.25 for the default Size=32, finer than 50, so a 50-unit bucket collapsed two
            // distinct vortices in neighbouring cells onto one id. Per-cell buckets are unique.
            const float CellSpacing = 1000.0f / static_cast<float>(FMath::Max(1, Size));
            const uint32 QX = static_cast<uint32>(FMath::RoundToInt(NewVor.Position.X / CellSpacing));
            const uint32 QY = static_cast<uint32>(FMath::RoundToInt(NewVor.Position.Y / CellSpacing));
            NewVor.Id = FGuid(GetTypeHash(GetRealmId().ToString()), QX, QY, GetSimulationVersion());
        }
        StableVortices.Add(NewVor);
    }

    FluidsState->ActiveVortices = StableVortices;
}
