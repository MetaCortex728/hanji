// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "HanjiCamera.generated.h"

/**
 * A fixed-angle quarter-view camera that follows a target. It owns zoom and pixel snapping together,
 * which is why it exists: the zoom steps change the size of a screen pixel in the world, and snapping
 * must use that size. Orthographic by default; the view width is the world width the screen shows at 100 %.
 */
UCLASS(ClassGroup = (Hanji))
class HANJI_API AHanjiCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	AHanjiCamera();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	FRotator ViewRotation = FRotator(-30.0, 45.0, 0.0);

	/** World width shown at zoom 1, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "1"))
	float ViewWidth = 1850.0f;

	/** Zoom steps as view-width ratios: 0.9 closer, 1.1 wider. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	TArray<float> ZoomSteps = {0.9f, 1.0f, 1.1f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "0"))
	float ZoomHalfLife = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "0"))
	float FollowHalfLife = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "1"))
	float Distance = 1500.0f;

	/** Rounds the camera to the screen-pixel lattice, so the scene does not shimmer as it follows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	bool bPixelSnap = true;

	/** Width of the pixel-art screen in art pixels at zoom 1; one screen pixel = ViewWidth * zoom / this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "1"))
	int32 ArtPixelsWide = 640;

	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void SetFollowTarget(AActor* Target, FVector FocusOffset = FVector::ZeroVector);

	/** Jumps to the target with no easing (level start, teleports). */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void SnapToTarget();

	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void SetZoomStep(int32 Step);

	/** Next zoom step, wrapping. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void CycleZoom();

	UFUNCTION(BlueprintPure, Category = "Hanji") int32 GetZoomStep() const { return ZoomStep; }
	UFUNCTION(BlueprintPure, Category = "Hanji") float GetZoom() const;
	/** World size of one screen pixel at the current zoom. */
	UFUNCTION(BlueprintPure, Category = "Hanji") float GetWorldPerPixel() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") FVector GetFocus() const { return Focus; }

	/** Advances the camera; Tick calls it, tests call it directly. */
	void Step(float DeltaSeconds);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

private:
	void Apply();

	UPROPERTY(Transient)
	TObjectPtr<AActor> Target;

	FVector Offset = FVector::ZeroVector;
	FVector Focus = FVector::ZeroVector;
	int32 ZoomStep = 1;
	double Zoom = 1.0;
	double ZoomTarget = 1.0;
};
