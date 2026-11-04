#pragma once

#include "CoreMinimal.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "StrideClock.h"
#include "LocomotionStop.h"
#include "PoseAtlas.h"
#include "PixelProbe.generated.h"

class AGyeolQuarterViewCamera;

// Transient lab sprites. File-based previews do not replace imported/cooked art assets.
UCLASS()
class GYEOLLAB_API UGyeolProbeSprite : public UPaperSprite
{
    GENERATED_BODY()
public:
    bool InitializeProbe(double UnitsPerPixel);
    bool InitializeImage(const FString& Path, double WorldHeight);
    bool InitializeAtlasFrame(UTexture2D* Texture, const FIntRect& Rect, double UnitsPerPixel, const FVector2D& Pivot);
    FIntRect SourceRect;
    FVector2D WorldExtent = FVector2D::ZeroVector;
private:
    bool InitializeTexture(UTexture2D* Texture, const FIntRect& Rect, double UnitsPerSourcePixel, bool bLit, const FVector2D* Pivot = nullptr);
};

UCLASS()
class GYEOLLAB_API UGyeolPixelProbeComponent : public UPaperSpriteComponent
{
    GENERATED_BODY()
public:
    UGyeolPixelProbeComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    bool InitializeProbe(AGyeolQuarterViewCamera* InCamera, bool bInSnap, double Scale);
    bool InitializeImage(AGyeolQuarterViewCamera* InCamera, bool bInSnap, const FString& Path, double PixelHeight);
    bool InitializeAtlas(AGyeolQuarterViewCamera* InCamera, bool bInSnap, const FString& Directory);
    bool SetPose(FName Name, const FVector& Direction = FVector::ZeroVector);
    FName ResolveDashPose(FName Fallback, FVector& Direction, bool bDash, bool bNeutral,
        uint64 Instance, double Elapsed, double GameTime, double DashSeconds);
    bool GetContactPoint(FName Pose, const FVector& Direction, FVector& Out) const;
    bool HasAtlas() const { return PoseSprites.Num() > 0; }
    int32 GetViewCount() const { return AtlasViews; }
    FName GetPoseKey() const { return CurrentPoseKey; }
    bool HasWalking() const { return bHasWalking; }
    bool HasAttackTransitions() const { return bHasAttackTransitions; }
    bool HasMovementEntry() const { return bHasMovementEntry; }
    bool HasRepresentativeStrike() const { return bHasRepresentativeStrike; }
    bool HasRepresentativeStrikeEntry() const { return bHasRepresentativeStrikeEntry; }
    const GyeolPose::FNativeStrike& GetNativeStrike() const { return NativeStrike; }
    const GyeolPose::FNativeStrike& GetNativeKick() const { return NativeKick; }
    int32 GetFacingSector() const { return FacingSector; }
    int32 GetDisplayedWalkPhase() const { return DisplayedWalkPhase; }
    int32 GetDisplayedStopBucket() const { return StopTransition.GetDisplayedStep() != INDEX_NONE ? StopTransition.Bucket : INDEX_NONE; }
    int32 GetDisplayedStopStep() const { return StopTransition.GetDisplayedStep(); }
    int32 GetWalkPoseCount() const { return bHasWalking ? Stride.PoseCount : 0; }
    double GetStrideDistance() const { return Stride.Distance; }
    void ResetLocomotion(bool bResetDash = true)
    {
        Stride.Reset(); StopTransition.Reset(); DisplayedWalkPhase = INDEX_NONE;
        if (bResetDash) { DashPlayback.Reset(); }
    }
    UPROPERTY(EditAnywhere, Category="Prototype") double WalkCycleCm = 180.0;
    TSet<FName> SeenPoses;
    TSet<FName> SeenPoseKeys;
    bool SetLit(bool bLit);
    bool InitializeNormalMap(const FString& Path, bool bFlatControl = false, bool bDebugVectors = false);
    bool HasNormalMap() const { return IsValid(NormalMaterial); }
    bool InitializeOcclusionHint(bool bDiagnosticMask);
    UGyeolProbeSprite* GetLabSprite();
    bool bSnap = false;
private:
    bool DisplayPose(FName Name, const FVector& Direction);
    UPROPERTY(Transient)
    TObjectPtr<AGyeolQuarterViewCamera> Camera;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> NormalMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OcclusionMaterial;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UGyeolProbeSprite>> PoseSprites;
    FVector AtlasDepthOffset = FVector::ZeroVector;
    TMap<FName, FVector2D> ContactAnchors;
    FIntPoint AtlasPivot = FIntPoint::ZeroValue;
    double AtlasUnitsPerPixel = 0.0;
    int32 AtlasViews = 1;
    int32 FacingSector = 0;
    FName CurrentPoseKey;
    FName RequestedPose = TEXT("idle");
    FVector RequestedDirection = FVector::ZeroVector;
    GyeolPose::FStrideClock Stride;
    GyeolPose::FLocomotionStop StopTransition;
    GyeolPose::FNativeStrike NativeStrike;
    GyeolPose::FNativeStrike NativeKick;
    GyeolPose::FDashPlayback DashPlayback;
    FVector DashDirection = FVector::ZeroVector;
    FVector NeutralDirection = FVector::ZeroVector;
    bool bHasPhasedDash = false;
    bool bHasStopTransitions = false;
    bool bHasWalking = false;
    bool bHasAttackTransitions = false;
    bool bHasMovementEntry = false;
    bool bHasRepresentativeStrike = false;
    bool bHasRepresentativeStrikeEntry = false;
    int32 DisplayedWalkPhase = INDEX_NONE;
};
