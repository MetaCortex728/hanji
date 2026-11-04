#pragma once

#include "CoreMinimal.h"

namespace GyeolQuarterView
{
    inline bool IsStudyResolutionSupported(int32 Width, bool bNativeAppearance)
    {
        return bNativeAppearance ? Width == 1920 : Width == 640 || Width == 960;
    }

    struct FZoom
    {
        double Current = 1.0;
        double Target = 1.0;

        void Set(double Value)
        {
            if (FMath::IsFinite(Value)) { Target = FMath::Clamp(Value, 0.9, 1.1); }
        }
        void Tick(double Seconds)
        {
            if (!FMath::IsFinite(Seconds) || Seconds <= 0.0) { return; }
            const double Alpha = 1.0 - FMath::Exp(-0.6931471805599453 * Seconds / 0.06);
            Current = FMath::Lerp(Current, Target, Alpha);
            if (FMath::Abs(Current - Target) < 0.0001) { Current = Target; }
        }
        // The preference is a view-width ratio: 90% closer, 110% wider.
        double Width(double BaseWidth) const { return BaseWidth * Current; }
    };

    inline FVector2D ZoomFocusBounds(const FVector2D& Base, double BaseWidth, double RenderWidth)
    {
        const double ExtraHalfWidth=FMath::Max(0.0, RenderWidth-BaseWidth)*.5;
        return FVector2D(FMath::Max(0.0,Base.X-ExtraHalfWidth), FMath::Max(0.0,Base.Y-ExtraHalfWidth*9.0/16.0));
    }

    inline FQuat SpriteFacingRotation(const FRotator& ViewRotation)
    {
        const FRotationMatrix Basis(ViewRotation);
        // Paper2D's front normal is local -Y. The lab quad reverses X to preserve image handedness.
        return FRotationMatrix::MakeFromXZ(-Basis.GetUnitAxis(EAxis::Y), Basis.GetUnitAxis(EAxis::Z)).ToQuat();
    }

    inline FVector SnapViewPlane(const FVector& Point, const FVector& Origin,
        const FRotator& ViewRotation, double UnitsPerPixel)
    {
        if (UnitsPerPixel <= 0.0)
        {
            return Point;
        }
        const FRotationMatrix Basis(ViewRotation);
        FVector Result = Point;
        for (EAxis::Type Axis : {EAxis::Y, EAxis::Z})
        {
            const FVector Direction = Basis.GetUnitAxis(Axis);
            const double Position = FVector::DotProduct(Point - Origin, Direction);
            Result += Direction * (FMath::RoundToDouble(Position / UnitsPerPixel) * UnitsPerPixel - Position);
        }
        return Result;
    }

    inline FVector Follow(const FVector& Current, const FVector& Target,
        double DeltaSeconds, double HalfLifeSeconds)
    {
        if (DeltaSeconds <= 0.0)
        {
            return Current;
        }
        if (HalfLifeSeconds <= 0.0)
        {
            return Target;
        }
        const double Alpha = 1.0 - FMath::Exp(-0.6931471805599453 * DeltaSeconds / HalfLifeSeconds);
        return FMath::Lerp(Current, Target, Alpha);
    }

    inline FVector SnapGroundAnchor(const FVector& Foot, const FVector& Origin,
        const FRotator& ViewRotation, double UnitsPerPixel)
    {
        FVector Result = SnapViewPlane(Foot, Origin, ViewRotation, UnitsPerPixel);
        const FVector Forward = ViewRotation.Vector();
        if (!FMath::IsNearlyZero(Forward.Z))
        {
            // Depth-only correction keeps the snapped projection but avoids sinking feet into the floor.
            Result += Forward * ((Foot.Z - Result.Z) / Forward.Z);
        }
        return Result;
    }

    inline FVector GroundDirection(const FVector2D& ScreenInput, double ViewYaw)
    {
        const FRotationMatrix Basis(FRotator(0.0, ViewYaw, 0.0));
        const FVector Direction = Basis.GetUnitAxis(EAxis::X) * ScreenInput.Y
            + Basis.GetUnitAxis(EAxis::Y) * ScreenInput.X;
        return Direction.GetClampedToMaxSize(1.0);
    }
}
