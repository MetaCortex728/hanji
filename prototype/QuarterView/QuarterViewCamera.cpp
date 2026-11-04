#include "QuarterViewCamera.h"

#include "QuarterViewMath.h"
#include "Camera/CameraComponent.h"

AGyeolQuarterViewCamera::AGyeolQuarterViewCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    UCameraComponent* Camera = GetCameraComponent();
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
    Camera->OrthoWidth = ViewWidth;
    Camera->AspectRatio = 16.0f / 9.0f;
    Camera->bConstrainAspectRatio = true;
    Camera->SetAutoCalculateOrthoPlanes(true);
}

void AGyeolQuarterViewCamera::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
    GetCameraComponent()->SetOrthoWidth(float(FMath::Max(1.0, GetRenderWidth())));
    SetActorRotation(ViewRotation);
}

void AGyeolQuarterViewCamera::SetFollowTarget(AActor* Target, FVector FocusOffset)
{
    if (IsValid(FollowTarget))
    {
        RemoveTickPrerequisiteActor(FollowTarget);
    }
    FollowTarget = Target;
    TargetOffset = FocusOffset;
    if (IsValid(FollowTarget))
    {
        AddTickPrerequisiteActor(FollowTarget);
        SnapToTarget();
    }
}

void AGyeolQuarterViewCamera::SnapToTarget()
{
    if (IsValid(FollowTarget))
    {
        FocusLocation = ClampRoomFocus(FollowTarget->GetActorLocation() + TargetOffset);
        ApplyView();
    }
}

void AGyeolQuarterViewCamera::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsValid(FollowTarget))
    {
        return;
    }
    Zoom.Tick(DeltaSeconds);
    FocusLocation = ClampRoomFocus(GyeolQuarterView::Follow(FocusLocation,
        ClampRoomFocus(FollowTarget->GetActorLocation() + TargetOffset), DeltaSeconds, FollowHalfLife));
    ApplyView();
}

void AGyeolQuarterViewCamera::ApplyView()
{
    GetCameraComponent()->SetOrthoWidth(float(FMath::Max(1.0, GetRenderWidth())));
    // Translation follows the target; target rotation never rotates the view.
    const FVector RenderFocus = bPixelSnap
        ? GyeolQuarterView::SnapViewPlane(FocusLocation, FVector::ZeroVector, ViewRotation, GetRenderUnitsPerPixel())
        : FocusLocation;
    SetActorLocationAndRotation(RenderFocus - ViewRotation.Vector() * ViewDistance, ViewRotation);
}

FVector AGyeolQuarterViewCamera::ClampRoomFocus(const FVector& Position) const
{
    if (FocusHalfExtent.X <= 0 || FocusHalfExtent.Y <= 0) { return Position; }
    const FRotationMatrix Basis(ViewRotation);
    const FVector Right=Basis.GetUnitAxis(EAxis::Y), Up=Basis.GetUnitAxis(EAxis::Z);
    const FVector GroundForward=FRotationMatrix(FRotator(0,ViewRotation.Yaw,0)).GetUnitAxis(EAxis::X);
    const double X=FVector::DotProduct(Position,Right), Y=FVector::DotProduct(Position,Up);
    const double VerticalScale=FVector::DotProduct(GroundForward,Up);
    // Zooming out consumes authored backdrop margin; zooming in never unlocks extra room travel.
    const FVector2D Limit=GyeolQuarterView::ZoomFocusBounds(FocusHalfExtent,ViewWidth,GetRenderWidth());
    return FMath::Abs(VerticalScale)>1.e-6 ? Position
        + Right*(FMath::Clamp(X,-Limit.X,Limit.X)-X)
        + GroundForward*((FMath::Clamp(Y,-Limit.Y,Limit.Y)-Y)/VerticalScale) : Position;
}
