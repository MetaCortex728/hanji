// SPDX-License-Identifier: Apache-2.0
#include "HanjiCamera.h"

#include "Camera/CameraComponent.h"
#include "HanjiCoreIncludes.h"

AHanjiCamera::AHanjiCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
	GetCameraComponent()->bConstrainAspectRatio = false;
}

void AHanjiCamera::BeginPlay()
{
	Super::BeginPlay();
	SetZoomStep(ZoomSteps.IndexOfByPredicate([](float Z) { return FMath::IsNearlyEqual(Z, 1.0f); }));
	Zoom = ZoomTarget;
	SnapToTarget();
}

void AHanjiCamera::SetFollowTarget(AActor* InTarget, FVector FocusOffset)
{
	Target = InTarget;
	Offset = FocusOffset;
}

void AHanjiCamera::SnapToTarget()
{
	if (Target) Focus = Target->GetActorLocation() + Offset;
	Apply();
}

void AHanjiCamera::SetZoomStep(int32 InStep)
{
	if (ZoomSteps.Num() == 0) return;
	ZoomStep = FMath::Clamp(InStep, 0, ZoomSteps.Num() - 1);
	ZoomTarget = ZoomSteps[ZoomStep];
}

void AHanjiCamera::CycleZoom()
{
	if (ZoomSteps.Num() > 0) SetZoomStep((ZoomStep + 1) % ZoomSteps.Num());
}

float AHanjiCamera::GetZoom() const { return static_cast<float>(Zoom); }

float AHanjiCamera::GetWorldPerPixel() const
{
	return static_cast<float>(ViewWidth * Zoom / FMath::Max(ArtPixelsWide, 1));
}

void AHanjiCamera::Step(float DeltaSeconds)
{
	hanji::Zoom Z;
	Z.current = Zoom;
	Z.min = -1e9;
	Z.max = 1e9;
	Z.half_life_s = ZoomHalfLife;
	Z.set(ZoomTarget);
	Z.tick(DeltaSeconds);
	Zoom = Z.current;
	if (Target)
	{
		Focus = HanjiConvert::Vector(hanji::follow(HanjiConvert::Vec(Focus), HanjiConvert::Vec(Target->GetActorLocation() + Offset),
			DeltaSeconds, FollowHalfLife));
	}
	Apply();
}

void AHanjiCamera::Apply()
{
	const hanji::ViewRotation View = HanjiConvert::View(ViewRotation);
	// The logical focus stays continuous; only the rendered camera position is snapped.
	FVector Shown = Focus;
	if (bPixelSnap)
	{
		Shown = HanjiConvert::Vector(hanji::snap_view_plane(HanjiConvert::Vec(Focus), {}, View, GetWorldPerPixel()));
	}
	SetActorLocationAndRotation(Shown - ViewRotation.Vector() * Distance, ViewRotation);
	GetCameraComponent()->SetOrthoWidth(static_cast<float>(ViewWidth * Zoom));
}

void AHanjiCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Step(DeltaSeconds);
}
