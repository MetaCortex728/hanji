#pragma once

#include "CoreMinimal.h"

namespace GyeolPose
{
    // Visual settling only. Movement and action input never wait for this clock.
    struct FLocomotionStop
    {
        int32 LastWalk = INDEX_NONE;
        int32 Bucket = INDEX_NONE;
        double Elapsed = 0;

        void Reset() { LastWalk = Bucket = INDEX_NONE; Elapsed = 0; }

        int32 GetDisplayedStep() const
        {
            return Bucket >= 0 && Bucket < 8 && Elapsed + 1.e-7 < .1
                ? FMath::FloorToInt((Elapsed + 1.e-7) * 60) : INDEX_NONE;
        }

        FName Sample(int32 Walk, double DeltaTime)
        {
            if (Walk >= 0 && Walk < 48)
            {
                LastWalk = Walk;
                Bucket = INDEX_NONE;
                Elapsed = 0;
                return FName(*FString::Printf(TEXT("walk%d"), Walk));
            }
            if (LastWalk != INDEX_NONE)
            {
                Bucket = ((LastWalk + 3) / 6) % 8;
                LastWalk = INDEX_NONE;
                Elapsed = 0;
            }
            else if (Bucket != INDEX_NONE && FMath::IsFinite(DeltaTime) && DeltaTime > 0)
            { Elapsed += DeltaTime; }
            if (Bucket != INDEX_NONE)
            {
                // The exported clip contains six 60Hz samples, ending at exact idle.
                if (Elapsed + 1.e-7 < .1)
                {
                    const int32 Step = FMath::FloorToInt((Elapsed + 1.e-7) * 60);
                    return FName(*FString::Printf(TEXT("stop%d_%d"), Bucket, Step));
                }
                Bucket = INDEX_NONE;
            }
            return FName(TEXT("idle"));
        }
    };
}
