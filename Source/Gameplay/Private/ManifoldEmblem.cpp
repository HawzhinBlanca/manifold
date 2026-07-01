// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "ManifoldEmblem.h"
#include "Engine/Texture2D.h"

namespace ManifoldEmblem
{
    void Render(TArray<FColor>& Px, int32 Size)
    {
        Size = FMath::Max(Size, 16);
        Px.SetNumUninitialized(Size * Size);

        const float Center = (Size - 1) * 0.5f;
        const float Radius = Size * 0.5f;

        // Deep indigo field with a subtle radial vignette toward black at the edges.
        // FColor constructor is (R, G, B, A).
        const FColor Gold(255, 205, 70, 255);       // warm gold linework
        const FColor GoldDim(150, 120, 40, 255);    // dimmer gold for wave spokes

        const float OuterR = Radius * 0.82f;
        const float InnerR = OuterR * 2.0f / 3.0f;  // the 3:2 resonance ratio
        const float SpiralA = Radius * 0.020f;
        const int32 Spokes = 12;

        for (int32 y = 0; y < Size; ++y)
        {
            for (int32 x = 0; x < Size; ++x)
            {
                const float dx = x - Center;
                const float dy = y - Center;
                const float Dist = FMath::Sqrt(dx * dx + dy * dy);
                const float Ang = FMath::Atan2(dy, dx); // -PI..PI

                // Background: indigo -> black vignette. FColor ctor is (R, G, B, A),
                // so blue is the LARGEST ramp to read as deep indigo (not reddish).
                const float V = FMath::Clamp(1.0f - Dist / Radius, 0.0f, 1.0f);
                FColor Col(
                    static_cast<uint8>(8 * V + 2),    // R
                    static_cast<uint8>(12 * V + 3),   // G
                    static_cast<uint8>(24 * V + 4),   // B
                    255);

                // Two resonance rings at radii in a 3:2 ratio.
                const float RingW = FMath::Max(1.0f, Size / 160.0f);
                if (FMath::Abs(Dist - OuterR) < RingW || FMath::Abs(Dist - InnerR) < RingW)
                {
                    Col = Gold;
                }

                // Central vortex spiral (Archimedean, a few turns).
                for (int32 k = 0; k < 3; ++k)
                {
                    const float Theta = (Ang + PI) + 2.0f * PI * k;
                    const float SR = SpiralA * Theta;
                    if (SR > 1.0f && SR < InnerR * 0.92f && FMath::Abs(Dist - SR) < RingW * 0.9f)
                    {
                        Col = Gold;
                        break;
                    }
                }

                // Radial wave spokes between the rings.
                if (Dist > InnerR + RingW * 2.0f && Dist < OuterR - RingW * 2.0f)
                {
                    const float A01 = (Ang + PI) / (2.0f * PI);
                    const float Nearest = FMath::Abs(FMath::Frac(A01 * Spokes) - 0.5f);
                    if (Nearest > 0.44f) // near a spoke boundary
                    {
                        Col = GoldDim;
                    }
                }

                Px[y * Size + x] = Col;
            }
        }
    }

    UTexture2D* CreateTexture(int32 Size)
    {
        // Keep in lockstep with Render()'s internal clamp so the texture dimensions
        // match the pixel buffer exactly (otherwise the memcpy below would overflow
        // the mip for Size < 16).
        Size = FMath::Max(Size, 16);

        TArray<FColor> Px;
        Render(Px, Size);

        UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
        if (!Texture)
        {
            return nullptr;
        }
        Texture->SRGB = true;
#if WITH_EDITORONLY_DATA
        Texture->MipGenSettings = TMGS_NoMipmaps;
#endif

        FTexturePlatformData* PlatformData = Texture->GetPlatformData();
        if (PlatformData && PlatformData->Mips.Num() > 0)
        {
            void* Data = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
            FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
            PlatformData->Mips[0].BulkData.Unlock();
            Texture->UpdateResource();
        }
        return Texture;
    }
}
