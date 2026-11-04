#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "QuarterViewLab.generated.h"

class AGyeolQuarterViewCamera;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UFloatingPawnMovement;
class UGyeolPixelProbeComponent;
class ADirectionalLight;
class APointLight;
class AStaticMeshActor;
struct FInputActionValue;

UCLASS()
class GYEOLLAB_API AGyeolQuarterViewLabPawn : public APawn
{
    GENERATED_BODY()

public:
    AGyeolQuarterViewLabPawn();
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual UPawnMovementComponent* GetMovementComponent() const override;
    void InjectMove(const FVector2D& Direction);
    void SetLocomotionPreview(UGyeolPixelProbeComponent* Sprite, float Speed);
    int32 GetMoveEventCount() const { return MoveEventCount; }

    UPROPERTY(Transient)
    TObjectPtr<AGyeolQuarterViewCamera> FollowCamera;

protected:
    virtual void Move(const FInputActionValue& Value);
    UInputAction* GetMoveAction() const { return MoveAction; }

private:
    int32 MoveEventCount = 0;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UFloatingPawnMovement> Movement;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> MoveContext;

    UPROPERTY(Transient)
    TObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UGyeolPixelProbeComponent> LocomotionPreview;
};

UCLASS()
class GYEOLLAB_API AGyeolQuarterViewLabHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

UCLASS()
class GYEOLLAB_API AGyeolQuarterViewLabMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGyeolQuarterViewLabMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    virtual bool IncludeCalibrationObstacles() const { return true; }
    virtual void BuildScene();

private:
    void Capture(const FString& Name);
    void TickMotionProbe();
    void OnMotionCaptureProcessed();
    void TickLightingProbe(float DeltaSeconds);
    void OnLightingCaptureProcessed();
    bool bLightingProbe = false;
    bool bDanMotionStudy = false;
    bool bLightingPrepared = false;
    int32 LightingStage = 0;
    int32 LightingFrames = 0;
    double LightingTime = 0.0;
    FString LightingCsv;
    FString CaptureTag;
    double GridWidth = 2.0;
    bool bMotionProbe = false;
    bool bCapturePending = false;
    bool bPreviousFixedTimeStep = false;
    double PreviousFixedDelta = 0.0;
    double CaptureRequestedAt = 0.0;
    int32 MotionWarmup = 0;
    int32 MotionFrame = 0;
    FString MotionCsv;
    FDelegateHandle CaptureProcessedHandle;

    UPROPERTY(Transient)
    TObjectPtr<UGyeolPixelProbeComponent> PixelProbe;

    UPROPERTY(Transient)
    TObjectPtr<AGyeolQuarterViewLabPawn> TestPawn;

    UPROPERTY(Transient)
    TObjectPtr<AGyeolQuarterViewCamera> TestCamera;

    UPROPERTY(Transient)
    TObjectPtr<ADirectionalLight> SceneSun;

    UPROPERTY(Transient)
    TObjectPtr<APointLight> SceneLamp;

    UPROPERTY(Transient)
    TObjectPtr<AStaticMeshActor> LightingOccluder;

    bool bSmokeTest = false;
    bool bSmokePassed = false;
    float SmokeTime = 0.0f;
    int32 SmokeStep = 0;
    FVector StartPosition = FVector::ZeroVector;
};
