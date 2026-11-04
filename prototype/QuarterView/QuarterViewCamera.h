#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "QuarterViewMath.h"
#include "QuarterViewCamera.generated.h"

UCLASS()
class GYEOLLAB_API AGyeolQuarterViewCamera : public ACameraActor
{
    GENERATED_BODY()

public:
    AGyeolQuarterViewCamera();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, Category = "Quarter View")
    void SetFollowTarget(AActor* Target, FVector FocusOffset = FVector::ZeroVector);

    UFUNCTION(BlueprintCallable, Category = "Quarter View")
    void SnapToTarget();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quarter View")
    FRotator ViewRotation = FRotator(-35.0, 45.0, 0.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quarter View", meta = (ClampMin = "1.0"))
    float ViewWidth = 1850.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quarter View", meta = (ClampMin = "1.0"))
    float ViewDistance = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quarter View", meta = (ClampMin = "0.0"))
    float FollowHalfLife = 0.08f;

    // Experimental render-only quantization; the logical follow position stays continuous.
    bool bPixelSnap = false;
    int32 InternalWidth = 640;
    // Optional room framing bounds in camera-plane centimetres; disabled in calibration labs.
    FVector2D FocusHalfExtent = FVector2D::ZeroVector;

    FVector GetFocusLocation() const { return FocusLocation; }
    void SetZoom(double Value) { Zoom.Set(Value); }
    double GetZoom() const { return Zoom.Current; }
    double GetTargetZoom() const { return Zoom.Target; }
    double GetRenderWidth() const { return Zoom.Width(ViewWidth); }
    double GetRenderUnitsPerPixel() const { return GetRenderWidth() / FMath::Max(1, InternalWidth); }

private:
    void ApplyView();
    FVector ClampRoomFocus(const FVector& Position) const;

    UPROPERTY(Transient)
    TObjectPtr<AActor> FollowTarget;

    FVector TargetOffset = FVector::ZeroVector;
    FVector FocusLocation = FVector::ZeroVector;
    GyeolQuarterView::FZoom Zoom;
};
