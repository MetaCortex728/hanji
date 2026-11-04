#pragma once

#include "CoreMinimal.h"

namespace GyeolPose
{
    // Visual-only distance-sampled gait. Collision/aim/actions never use this clock.
    struct FStrideClock
    {
        double CycleCm = 180.0;
        int32 PoseCount = 4;
        double Distance = 0.0;
        FVector Previous = FVector::ZeroVector;
        bool bSampled = false;

        void Reset() { Distance = 0.0; bSampled = false; }

        int32 Sample(const FVector& Location, bool bWalkingAllowed)
        {
            if (Location.ContainsNaN() || !FMath::IsFinite(CycleCm) || CycleCm <= 0.0 || PoseCount <= 0)
            { Reset(); return INDEX_NONE; }
            const double Travel = bSampled ? FVector::Dist2D(Previous, Location) : 0.0;
            Previous = Location;
            bSampled = true;
            if (!bWalkingAllowed || Travel > 100.0)
            { Distance = 0.0; return INDEX_NONE; }
            if (Travel <= 1.e-4) { return INDEX_NONE; }
            Distance = FMath::Fmod(Distance + Travel, CycleCm);
            return FMath::Clamp(FMath::FloorToInt(Distance * PoseCount / CycleCm), 0, PoseCount - 1);
        }
    };
}
