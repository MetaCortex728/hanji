#include "QuarterViewLab.h"

#include "QuarterViewCamera.h"
#include "QuarterViewMath.h"
#include "PixelProbe.h"
#include "PoseAtlas.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/HitResult.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuarterViewLab, Log, All);

AGyeolQuarterViewLabPawn::AGyeolQuarterViewLabPawn()
{
    UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
    // Planar movement does not resolve floor height; leave a 2cm contact margin.
    Capsule->InitCapsuleSize(28.0f, 83.0f);
    Capsule->SetCollisionProfileName(TEXT("Pawn"));
    SetRootComponent(Capsule);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMeshComponent* Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    Body->SetupAttachment(Capsule);
    Body->SetStaticMesh(Sphere.Object);
    Body->SetRelativeLocation(FVector(0.0, 0.0, -10.0));
    Body->SetRelativeScale3D(FVector(0.5, 0.5, 1.3));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    UStaticMeshComponent* Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderHead"));
    Head->SetupAttachment(Capsule);
    Head->SetStaticMesh(Sphere.Object);
    Head->SetRelativeLocation(FVector(0.0, 0.0, 68.0));
    Head->SetRelativeScale3D(FVector(0.34));
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->SetUpdatedComponent(Capsule);
    Movement->MaxSpeed = 500.0f;
    Movement->Acceleration = 12000.0f;
    Movement->Deceleration = 12000.0f;
    Movement->SetPlaneConstraintEnabled(true);
    Movement->SetPlaneConstraintNormal(FVector::UpVector);
    Movement->SetPlaneConstraintOrigin(FVector(0.0, 0.0, 85.0));
}

UPawnMovementComponent* AGyeolQuarterViewLabPawn::GetMovementComponent() const
{
    return Movement;
}

void AGyeolQuarterViewLabPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    APlayerController* Player = Cast<APlayerController>(GetController());
    if (!ensure(Enhanced && Player && Player->GetLocalPlayer()))
    {
        return;
    }
    InputSubsystem = Player->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!ensure(InputSubsystem))
    {
        return;
    }
    if (MoveContext)
    {
        InputSubsystem->RemoveMappingContext(MoveContext);
    }
    MoveAction = NewObject<UInputAction>(this);
    MoveAction->ValueType = EInputActionValueType::Axis2D;
    MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    MoveContext = NewObject<UInputMappingContext>(this);
    const auto Map = [this](FKey Key, bool bVertical, bool bNegative)
    {
        FEnhancedActionKeyMapping& Mapping = MoveContext->MapKey(MoveAction, Key);
        if (bNegative)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MoveContext));
        }
        if (bVertical)
        {
            UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MoveContext);
            Swizzle->Order = EInputAxisSwizzle::YXZ;
            Mapping.Modifiers.Add(Swizzle);
        }
    };
    Map(EKeys::W, true, false);
    Map(EKeys::Up, true, false);
    Map(EKeys::S, true, true);
    Map(EKeys::Down, true, true);
    Map(EKeys::A, false, true);
    Map(EKeys::Left, false, true);
    Map(EKeys::D, false, false);
    Map(EKeys::Right, false, false);
    Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGyeolQuarterViewLabPawn::Move);
    InputSubsystem->AddMappingContext(MoveContext, 0);
    UE_LOG(LogQuarterViewLab, Display, TEXT("Enhanced Input mapping and move binding ready."));
}

void AGyeolQuarterViewLabPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(InputSubsystem) && MoveContext)
    {
        InputSubsystem->RemoveMappingContext(MoveContext);
    }
    Super::EndPlay(EndPlayReason);
}

void AGyeolQuarterViewLabPawn::Move(const FInputActionValue& Value)
{
    ++MoveEventCount;
    const double Yaw = IsValid(FollowCamera) ? FollowCamera->ViewRotation.Yaw : 45.0;
    const FVector Direction = LocomotionPreview && IsValid(FollowCamera)
        ? GyeolPose::GroundFromScreen(Value.Get<FVector2D>(), FollowCamera->ViewRotation)
        : GyeolQuarterView::GroundDirection(Value.Get<FVector2D>(), Yaw);
    AddMovementInput(Direction.GetSafeNormal(), Direction.Size());
    if (LocomotionPreview && !Direction.IsNearlyZero()) { LocomotionPreview->SetPose(TEXT("idle"), Direction); }
    if (MoveEventCount == 10 && FParse::Param(FCommandLine::Get(), TEXT("GyeolCameraSmokeTest")))
    {
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(QuarterViewProbe), false, this);
        GetWorld()->SweepSingleByChannel(Hit, GetActorLocation(), GetActorLocation() + Direction * 100.0,
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(28.0f, 83.0f), Query);
        UE_LOG(LogQuarterViewLab, Display,
            TEXT("Move diagnostic: pending=%s velocity=%s updated=%s active=%d tick=%d ignored=%d blocker=%s penetration=%.3f"),
            *GetPendingMovementInputVector().ToString(), *Movement->Velocity.ToString(),
            *GetNameSafe(Movement->UpdatedComponent), Movement->IsActive(), Movement->IsComponentTickEnabled(),
            IsMoveInputIgnored(), *GetNameSafe(Hit.GetActor()), Hit.PenetrationDepth);
    }
}

void AGyeolQuarterViewLabPawn::SetLocomotionPreview(UGyeolPixelProbeComponent* Sprite, float Speed)
{
    LocomotionPreview = Sprite;
    Movement->MaxSpeed = Speed;
}

void AGyeolQuarterViewLabPawn::InjectMove(const FVector2D& Direction)
{
    if (IsValid(InputSubsystem) && MoveAction)
    {
        InputSubsystem->InjectInputForAction(MoveAction, FInputActionValue(Direction), {}, {});
    }
}

void AGyeolQuarterViewLabHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
        return;
    }
    DrawRect(FLinearColor(0.025f, 0.03f, 0.035f, 0.88f), 12.0f, 12.0f, 670.0f, 94.0f);
    DrawText(TEXT("GYEOL / QUARTER-VIEW CAMERA LAB"), FLinearColor::White, 24.0f, 22.0f);
    const AGyeolQuarterViewLabPawn* Pawn = Cast<AGyeolQuarterViewLabPawn>(GetOwningPawn());
    const AGyeolQuarterViewCamera* Camera = Pawn ? Pawn->FollowCamera.Get() : nullptr;
    const FString ViewLabel = Camera
        ? FString::Printf(TEXT("WASD / arrows | pitch %.1f, yaw %.1f | width %.1f cm"),
            Camera->ViewRotation.Pitch, Camera->ViewRotation.Yaw, Camera->ViewWidth)
        : TEXT("Waiting for camera");
    DrawText(ViewLabel,
        FLinearColor(0.7f, 0.9f, 0.85f), 24.0f, 44.0f);
    const bool bAppearance = FParse::Param(FCommandLine::Get(), TEXT("GyeolNativeAppearance"));
    const bool bDanMotion = FParse::Param(FCommandLine::Get(), TEXT("GyeolDanMotionStudy"));
    DrawText(bDanMotion ? TEXT("Dan locomotion study: 8 directions. Combat unavailable.")
        : bAppearance ? TEXT("Dan appearance fixture: one static direction, no combat or animation.")
        : TEXT("Calibration lab. No residual-offset compositor / combat yet."),
        FLinearColor(0.85f, 0.85f, 0.85f), 24.0f, 66.0f);
}

AGyeolQuarterViewLabMode::AGyeolQuarterViewLabMode()
{
    DefaultPawnClass = AGyeolQuarterViewLabPawn::StaticClass();
    HUDClass = AGyeolQuarterViewLabHUD::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
}

void AGyeolQuarterViewLabMode::StartPlay()
{
    bSmokeTest = FParse::Param(FCommandLine::Get(), TEXT("GyeolCameraSmokeTest"));
    bMotionProbe = FParse::Param(FCommandLine::Get(), TEXT("GyeolMotionProbe"));
    bLightingProbe = FParse::Param(FCommandLine::Get(), TEXT("GyeolLightingProbe"));
    bDanMotionStudy = FParse::Param(FCommandLine::Get(), TEXT("GyeolDanMotionStudy"));
    FString SpriteInput;
    FParse::Value(FCommandLine::Get(), TEXT("GyeolSpriteInput="), SpriteInput);
    FString NormalInput;
    FParse::Value(FCommandLine::Get(), TEXT("GyeolNormalInput="), NormalInput);
    if (!NormalInput.IsEmpty() && (!bLightingProbe || SpriteInput.IsEmpty()
        || !NormalInput.StartsWith(TEXT("Saved/SpriteInputs/")) || NormalInput.Contains(TEXT(".."))))
    {
        UE_LOG(LogQuarterViewLab, Error, TEXT("Normal data is restricted to the file-based lighting probe."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    FString PoseInput;
    FParse::Value(FCommandLine::Get(), TEXT("GyeolPoseInput="), PoseInput);
    if (bDanMotionStudy && (PoseInput.IsEmpty() || !SpriteInput.IsEmpty() || bLightingProbe || bSmokeTest))
    {
        UE_LOG(LogQuarterViewLab, Error, TEXT("Dan motion study requires a dedicated locomotion atlas."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    const bool bNormalAtlas = FParse::Param(FCommandLine::Get(), TEXT("GyeolNormalAtlas"));
    if (bNormalAtlas && (PoseInput.IsEmpty() || !NormalInput.IsEmpty()))
    {
        UE_LOG(LogQuarterViewLab, Error, TEXT("Normal atlas requires pose input only."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    double SpriteHeight = 48.0;
    const bool bNativeAppearance = FParse::Param(FCommandLine::Get(), TEXT("GyeolNativeAppearance"));
    const bool bRepresentativeStrike = FParse::Param(FCommandLine::Get(), TEXT("GyeolDanStudy"))
        && FParse::Param(FCommandLine::Get(), TEXT("GyeolRepresentativeStrike"));
    FParse::Value(FCommandLine::Get(), TEXT("GyeolSpriteHeight="), SpriteHeight);
    if ((bLightingProbe && (SpriteInput.IsEmpty() || bMotionProbe || bSmokeTest)) ||
        (bMotionProbe && !SpriteInput.IsEmpty()) ||
        (bNativeAppearance && (SpriteInput.IsEmpty() || !PoseInput.IsEmpty() || bMotionProbe || bSmokeTest)) ||
        !FMath::IsFinite(SpriteHeight) || SpriteHeight < 24.0 || SpriteHeight > (bNativeAppearance ? 256.0 : 128.0) ||
        (!SpriteInput.IsEmpty() && (!SpriteInput.StartsWith(TEXT("Saved/SpriteInputs/")) || SpriteInput.Contains(TEXT("..")))) ||
        (!PoseInput.IsEmpty() && (!SpriteInput.IsEmpty() || (bMotionProbe && !bDanMotionStudy) || bLightingProbe || bSmokeTest
            || !PoseInput.StartsWith(TEXT("Saved/PoseInputs/")) || PoseInput.Contains(TEXT("..")))))
    {
        UE_LOG(LogQuarterViewLab, Error, TEXT("Invalid sprite/lighting experiment parameters."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    FParse::Value(FCommandLine::Get(), TEXT("GyeolGridWidth="), GridWidth);
    if (!FMath::IsFinite(GridWidth) || GridWidth < 0.5 || GridWidth > 30.0)
    {
        UE_LOG(LogQuarterViewLab, Error, TEXT("Invalid grid width."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    FParse::Value(FCommandLine::Get(), TEXT("GyeolCaptureTag="), CaptureTag);
    for (TCHAR Character : CaptureTag)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Invalid capture tag; use letters, numbers, hyphens or underscores."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
    }
    Super::StartPlay();
    BuildScene();
    TestPawn = Cast<AGyeolQuarterViewLabPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!ensure(TestPawn))
    {
        if (bSmokeTest || bMotionProbe || bLightingProbe)
        {
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return;
    }
    TestPawn->SetActorLocation(FVector(0.0, 0.0, 85.0));
    TestCamera = GetWorld()->SpawnActor<AGyeolQuarterViewCamera>();
    double Pitch = TestCamera->ViewRotation.Pitch;
    float Width = TestCamera->ViewWidth;
    int32 ViewPercent = 100;
    int32 InternalWidth = TestCamera->InternalWidth;
    FParse::Value(FCommandLine::Get(), TEXT("GyeolViewPitch="), Pitch);
    FParse::Value(FCommandLine::Get(), TEXT("GyeolViewWidth="), Width);
    FParse::Value(FCommandLine::Get(), TEXT("GyeolViewPercent="), ViewPercent);
    FParse::Value(FCommandLine::Get(), TEXT("GyeolInternalWidth="), InternalWidth);
    if (!FMath::IsFinite(Pitch) || Pitch < -80.0 || Pitch > -10.0 ||
        !FMath::IsFinite(Width) || Width < 100.0f || Width > 10000.0f ||
        (ViewPercent != 90 && ViewPercent != 100 && ViewPercent != 110) ||
        !GyeolQuarterView::IsStudyResolutionSupported(InternalWidth, bNativeAppearance || bDanMotionStudy || bRepresentativeStrike) ||
        (bNormalAtlas && !FMath::IsNearlyEqual(Pitch, -30.0)))
    {
        UE_LOG(LogQuarterViewLab, Error, TEXT("Invalid camera experiment parameters."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    TestCamera->ViewRotation.Pitch = Pitch;
    TestCamera->ViewWidth = Width;
    TestCamera->SetZoom(ViewPercent / 100.0);
    TestCamera->InternalWidth = InternalWidth;
    TestCamera->bPixelSnap = FParse::Param(FCommandLine::Get(), TEXT("GyeolPixelSnap"));
    TestCamera->SetFollowTarget(TestPawn, FVector(0.0, 0.0, -85.0));
    TestPawn->FollowCamera = TestCamera;
    APlayerController* Player = CastChecked<APlayerController>(TestPawn->GetController());
    Player->bAutoManageActiveCameraTarget = false;
    Player->SetViewTarget(TestCamera);
    Player->SetInputMode(FInputModeGameOnly());
    if (bMotionProbe || !SpriteInput.IsEmpty() || !PoseInput.IsEmpty() || FParse::Param(FCommandLine::Get(), TEXT("GyeolPixelProbe")))
    {
        double Scale = 1.0;
        FParse::Value(FCommandLine::Get(), TEXT("GyeolProbeScale="), Scale);
        if (!FMath::IsFinite(Scale) || Scale < 0.5 || Scale > 2.0)
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Invalid probe scale."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        PixelProbe = NewObject<UGyeolPixelProbeComponent>(TestPawn);
        TestPawn->AddInstanceComponent(PixelProbe);
        PixelProbe->RegisterComponent();
        const bool bInitialized = !PoseInput.IsEmpty()
            ? PixelProbe->InitializeAtlas(TestCamera, TestCamera->bPixelSnap, FPaths::Combine(FPaths::ProjectDir(), PoseInput))
            : SpriteInput.IsEmpty()
            ? PixelProbe->InitializeProbe(TestCamera, TestCamera->bPixelSnap, Scale)
            : PixelProbe->InitializeImage(TestCamera, TestCamera->bPixelSnap,
                FPaths::Combine(FPaths::ProjectDir(), SpriteInput), SpriteHeight);
        if (!bInitialized)
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Probe initialization failed."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        if ((bNativeAppearance && !bLightingProbe) || bDanMotionStudy || bRepresentativeStrike)
        {
            PixelProbe->SetLit(false);
        }
        FString OcclusionHint;
        if (FParse::Value(FCommandLine::Get(), TEXT("GyeolOcclusionHint="), OcclusionHint) && OcclusionHint != TEXT("Off"))
        {
            if (!bRepresentativeStrike || (OcclusionHint != TEXT("On") && OcclusionHint != TEXT("Mask"))
                || !PixelProbe->InitializeOcclusionHint(OcclusionHint == TEXT("Mask")))
            {
                UE_LOG(LogQuarterViewLab, Error, TEXT("Player occlusion hint initialization failed."));
                FPlatformMisc::RequestExitWithStatus(false, 1);
                return;
            }
            UE_LOG(LogQuarterViewLab, Display, TEXT("Player occlusion hint ready: %s"), *OcclusionHint);
        }
        if (bDanMotionStudy)
        {
            if (PixelProbe->GetWalkPoseCount() < 4 || PixelProbe->GetWalkPoseCount() > 120)
            {
                UE_LOG(LogQuarterViewLab, Error, TEXT("Dan motion study needs 4-120 locomotion poses sampled at 60Hz."));
                FPlatformMisc::RequestExitWithStatus(false, 1);
                return;
            }
            TestPawn->SetLocomotionPreview(PixelProbe, PixelProbe->WalkCycleCm * 60.0 / PixelProbe->GetWalkPoseCount());
        }
        if (bNormalAtlas) { NormalInput = FPaths::Combine(PoseInput, TEXT("normal-proxy.png")); }
        if (!NormalInput.IsEmpty() && !PixelProbe->InitializeNormalMap(FPaths::Combine(FPaths::ProjectDir(), NormalInput),
            FParse::Param(FCommandLine::Get(), TEXT("GyeolFlatNormalControl")),
            FParse::Param(FCommandLine::Get(), TEXT("GyeolNormalDebug"))))
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Normal material initialization failed."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        if (bNormalAtlas)
        {
            UE_LOG(LogQuarterViewLab, Display, TEXT("Normal atlas ready: %s; vectors=%d flat=%d"), *NormalInput,
                FParse::Param(FCommandLine::Get(), TEXT("GyeolNormalDebug")), FParse::Param(FCommandLine::Get(), TEXT("GyeolFlatNormalControl")));
        }
        TArray<UStaticMeshComponent*> Placeholders;
        TestPawn->GetComponents(Placeholders);
        for (UStaticMeshComponent* Placeholder : Placeholders)
        {
            Placeholder->SetVisibility(false);
        }
        UE_LOG(LogQuarterViewLab, Display, TEXT("Probe ready: input=%s scale=%.4f snap=%d gridWidth=%.2fcm displayHeight=%.3fpx"),
            !PoseInput.IsEmpty() ? *PoseInput : SpriteInput.IsEmpty() ? TEXT("native32x48") : *SpriteInput, Scale, TestCamera->bPixelSnap, GridWidth,
            SpriteInput.IsEmpty() ? 48.0 * Scale : SpriteHeight);
    }
    if (bMotionProbe)
    {
        PreviousFixedDelta = FApp::GetFixedDeltaTime();
        bPreviousFixedTimeStep = FApp::UseFixedTimeStep();
        FApp::SetFixedDeltaTime(1.0 / 60.0);
        FApp::SetUseFixedTimeStep(true);
        TestPawn->GetMovementComponent()->Deactivate();
        CaptureProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
            this, &AGyeolQuarterViewLabMode::OnMotionCaptureProcessed);
        MotionCsv = TEXT("frame,sim_seconds,pawn_x,pawn_y,pawn_z,focus_x,focus_y,focus_z,camera_x,camera_y,camera_z,visual_x,visual_y,visual_z,screen_x,screen_y\n");
        if (bDanMotionStudy)
        {
            MotionCsv = TEXT("frame,sector,pose,x,y,camera_x,camera_y,stride_cm,screen_x,screen_y\n");
        }
    }
    if (bLightingProbe)
    {
        TestPawn->GetMovementComponent()->Deactivate();
        CaptureProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
            this, &AGyeolQuarterViewLabMode::OnLightingCaptureProcessed);
        LightingCsv = TEXT("stage,name,screen_x,screen_y,width_cm,height_cm,source_x0,source_y0,source_x1,source_y1,light_x,light_y,light_z,point_intensity,lit,occluded\n");
        const FVector TowardCamera = -FVector(TestCamera->ViewRotation.Vector().X, TestCamera->ViewRotation.Vector().Y, 0.0).GetSafeNormal();
        LightingOccluder->SetActorLocation(TowardCamera * 45.0 + FVector(0.0, 0.0, 45.0));
        LightingOccluder->SetActorRotation(FRotator(0.0, TestCamera->ViewRotation.Yaw, 0.0));
    }
    StartPosition = TestPawn->GetActorLocation();
    UE_LOG(LogQuarterViewLab, Display, TEXT("Lab ready: pawn=%s camera=%s width=%.3f internal=%d cmPerPixel=%.6f tag=%s; saved maps untouched."),
        *StartPosition.ToString(), *TestCamera->GetActorRotation().ToString(), Width, InternalWidth,
        TestCamera->GetRenderUnitsPerPixel(), *CaptureTag);
}

void AGyeolQuarterViewLabMode::BuildScene()
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    const auto Box = [this, Cube, BaseMaterial](FVector Position, FVector Size, FLinearColor Color, bool bCollision = true)
    {
        AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Position, FRotator::ZeroRotator);
        UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
        Actor->SetActorScale3D(Size / 100.0);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Actor);
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Material);
        }
        return Actor;
    };
    Box(FVector(0.0, 0.0, -12.0), FVector(3200.0, 3200.0, 24.0), FLinearColor(0.12f, 0.2f, 0.18f));
    for (int32 Index = -7; Index <= 7; ++Index)
    {
        const double At = Index * 200.0;
        Box(FVector(At, 0.0, 0.5), FVector(GridWidth, 3000.0, 1.0), FLinearColor(0.24f, 0.35f, 0.3f), false);
        Box(FVector(0.0, At, 0.5), FVector(3000.0, GridWidth, 1.0), FLinearColor(0.24f, 0.35f, 0.3f), false);
    }
    if (IncludeCalibrationObstacles() && !(bDanMotionStudy && bMotionProbe))
    {
        Box(FVector(650.0, 650.0, 100.0), FVector(450.0, 35.0, 200.0), FLinearColor(0.45f, 0.48f, 0.5f));
        Box(FVector(-450.0, 300.0, 75.0), FVector(100.0, 350.0, 150.0), FLinearColor(0.28f, 0.33f, 0.38f));
        Box(FVector(200.0, -350.0, 40.0), FVector(100.0, 100.0, 80.0), FLinearColor(0.6f, 0.22f, 0.15f));
        Box(FVector(-700.0, -700.0, 150.0), FVector(55.0, 55.0, 300.0), FLinearColor(0.35f, 0.42f, 0.46f));
    }
    // The room edges are collision boundaries, not camera-follow constraints.
    Box(FVector(0.0, 1600.0, 100.0), FVector(3200.0, 30.0, 200.0), FLinearColor::Gray);
    Box(FVector(0.0, -1600.0, 100.0), FVector(3200.0, 30.0, 200.0), FLinearColor::Gray);
    Box(FVector(1600.0, 0.0, 100.0), FVector(30.0, 3200.0, 200.0), FLinearColor::Gray);
    Box(FVector(-1600.0, 0.0, 100.0), FVector(30.0, 3200.0, 200.0), FLinearColor::Gray);

    if (bLightingProbe)
    {
        LightingOccluder = Box(FVector::ZeroVector, FVector(16.0, 160.0, 90.0), FLinearColor(0.22f, 0.28f, 0.4f), false);
        LightingOccluder->SetActorHiddenInGame(true);
        LightingOccluder->GetStaticMeshComponent()->SetCastShadow(false);
    }
    SceneSun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 700.0), FRotator(-55.0, -30.0, 0.0));
    SceneSun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    SceneSun->GetLightComponent()->SetIntensity(3.0f);
    SceneLamp = GetWorld()->SpawnActor<APointLight>(FVector(200.0, -350.0, 230.0), FRotator::ZeroRotator);
    UPointLightComponent* Point = CastChecked<UPointLightComponent>(SceneLamp->GetLightComponent());
    Point->SetMobility(EComponentMobility::Movable);
    Point->SetIntensity(7000.0f);
    Point->SetAttenuationRadius(750.0f);
    Point->SetLightColor(FLinearColor(1.0f, 0.6f, 0.32f));
}

void AGyeolQuarterViewLabMode::Capture(const FString& Name)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QuarterViewLab"), CaptureTag);
    IFileManager::Get().MakeDirectory(*Directory, true);
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, Name + TEXT(".png")), false, false);
    UE_LOG(LogQuarterViewLab, Display, TEXT("Capture %s: pawn=%s focus=%s rotation=%s"), *Name,
        *TestPawn->GetActorLocation().ToString(), *TestCamera->GetFocusLocation().ToString(),
        *TestCamera->GetActorRotation().ToString());
    for (const TCHAR* NameOfSetting : { TEXT("r.ScreenPercentage"), TEXT("r.Upscale.Quality"),
        TEXT("r.AntiAliasingMethod"), TEXT("r.Tonemapper.MergeWithUpscale.Mode"),
        TEXT("r.SecondaryScreenPercentage.GameViewport") })
    {
        if (IConsoleVariable* Setting = IConsoleManager::Get().FindConsoleVariable(NameOfSetting))
        {
            UE_LOG(LogQuarterViewLab, Display, TEXT("Render setting %s=%s"), NameOfSetting, *Setting->GetString());
        }
    }
}

void AGyeolQuarterViewLabMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bLightingProbe && IsValid(TestPawn) && IsValid(TestCamera))
    {
        TickLightingProbe(DeltaSeconds);
        return;
    }
    if (bMotionProbe && IsValid(TestPawn) && IsValid(TestCamera))
    {
        TickMotionProbe();
        return;
    }
    if (!bSmokeTest || !IsValid(TestPawn) || !IsValid(TestCamera))
    {
        return;
    }
    SmokeTime += DeltaSeconds;
    // Separate phases so shader/capture stalls cannot skip the input interval.
    if (SmokeStep == 0 && SmokeTime >= 2.0f)
    {
        Capture(TEXT("01-stationary"));
        SmokeStep = 1;
        SmokeTime = 0.0f;
    }
    else if (SmokeStep == 1 && SmokeTime >= 0.5f)
    {
        SmokeStep = 2;
        SmokeTime = 0.0f;
    }
    else if (SmokeStep == 2)
    {
        TestPawn->InjectMove(FVector2D(0.6, 0.8));
        if ((SmokeTime >= 1.5f && TestPawn->GetMoveEventCount() >= 10) || SmokeTime >= 8.0f)
        {
            SmokeStep = 3;
            SmokeTime = 0.0f;
        }
    }
    else if (SmokeStep == 3 && SmokeTime >= 1.0f)
    {
        Capture(TEXT("02-followed"));
        const double Distance = FVector::Dist(StartPosition, TestPawn->GetActorLocation());
        const double Error = FVector::Dist(TestCamera->GetFocusLocation(),
            TestPawn->GetActorLocation() - FVector(0.0, 0.0, 85.0));
        bSmokePassed = Distance > 100.0 && Error < 5.0 && TestPawn->GetMoveEventCount() >= 10
            && TestCamera->GetActorRotation().Equals(TestCamera->ViewRotation, 0.01);
        UE_LOG(LogQuarterViewLab, Display, TEXT("FOLLOW %s: inputEvents=%d moved=%.2fcm settledError=%.3fcm fixedRotation=%s"),
            bSmokePassed ? TEXT("PASS") : TEXT("FAIL"), TestPawn->GetMoveEventCount(), Distance, Error,
            *TestCamera->GetActorRotation().ToString());
        SmokeStep = 4;
        SmokeTime = 0.0f;
    }
    else if (SmokeStep == 4 && SmokeTime >= 2.0f)
    {
        TestPawn->GetMovementComponent()->StopMovementImmediately();
        TestPawn->SetActorLocation(FVector(-200.0, 300.0, 85.0));
        TestCamera->SnapToTarget();
        SmokeStep = 5;
        SmokeTime = 0.0f;
    }
    else if (SmokeStep == 5)
    {
        TestPawn->InjectMove(FVector2D(0.70710678, -0.70710678));
        if (SmokeTime >= 1.2f)
        {
            SmokeStep = 6;
            SmokeTime = 0.0f;
        }
    }
    else if (SmokeStep == 6 && SmokeTime >= 1.0f)
    {
        Capture(TEXT("03-wall-contact"));
        const FVector Position = TestPawn->GetActorLocation();
        const bool bWallPassed = Position.X > -375.0 && Position.X < -330.0
            && FMath::Abs(Position.Y - 300.0) < 2.0 && FMath::Abs(Position.Z - 85.0) < 0.1;
        bSmokePassed &= bWallPassed;
        UE_LOG(LogQuarterViewLab, Display, TEXT("SMOKE %s: wallBlocked=%d position=%s"),
            bSmokePassed ? TEXT("PASS") : TEXT("FAIL"), bWallPassed, *Position.ToString());
        SmokeStep = 7;
        SmokeTime = 0.0f;
    }
    else if (SmokeStep == 7 && SmokeTime >= 2.0f)
    {
        FPlatformMisc::RequestExitWithStatus(false, bSmokePassed ? 0 : 1);
    }
}

void AGyeolQuarterViewLabMode::TickMotionProbe()
{
    if (++MotionWarmup <= 60)
    {
        return;
    }
    if (bCapturePending)
    {
        if (FPlatformTime::Seconds() - CaptureRequestedAt > 30.0)
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Motion capture callback timed out."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return;
    }
    const int32 CaptureCount = bDanMotionStudy ? 512 : 120;
    if (MotionFrame >= CaptureCount)
    {
        const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QuarterViewLab"), CaptureTag, TEXT("motion.csv"));
        const bool bSaved = FFileHelper::SaveStringToFile(MotionCsv, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogQuarterViewLab, Display, TEXT("MOTION %s: %d captures; fixed simulation step, not a performance benchmark."),
            bSaved ? TEXT("CAPTURE PASS") : TEXT("CAPTURE FAIL"), MotionFrame);
        FPlatformMisc::RequestExitWithStatus(false, bSaved ? 0 : 1);
        return;
    }
    if (bDanMotionStudy)
    {
        const int32 Sector = MotionFrame / 64;
        const int32 Phase = MotionFrame % 64;
        const double Angle = Sector * UE_PI / 4.0;
        const FVector Direction = GyeolPose::GroundFromScreen(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)),
            TestCamera->ViewRotation);
        if (Phase < 56)
        {
            TestPawn->SetActorLocation(TestPawn->GetActorLocation() + Direction * (PixelProbe->WalkCycleCm / PixelProbe->GetWalkPoseCount()));
            PixelProbe->SetPose(TEXT("idle"), Direction);
        }
    }
    else
    {
        const FRotationMatrix YawBasis(FRotator(0.0, TestCamera->ViewRotation.Yaw, 0.0));
        const double SinPitch = FMath::Sin(FMath::DegreesToRadians(FMath::Abs(TestCamera->ViewRotation.Pitch)));
        const FVector Direction = (YawBasis.GetUnitAxis(EAxis::Y) + YawBasis.GetUnitAxis(EAxis::X) / SinPitch).GetSafeNormal();
        const int32 MovingSteps = FMath::Clamp(MotionFrame - 19, 0, 80);
        TestPawn->SetActorLocation(StartPosition + Direction * (MovingSteps * 50.0 / 60.0));
    }
    bCapturePending = true;
    CaptureRequestedAt = FPlatformTime::Seconds();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QuarterViewLab"), CaptureTag);
    IFileManager::Get().MakeDirectory(*Directory, true);
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,
        FString::Printf(TEXT("motion-%03d.png"), MotionFrame)), false, false);
}

void AGyeolQuarterViewLabMode::OnMotionCaptureProcessed()
{
    if (!bCapturePending || !IsValid(PixelProbe))
    {
        return;
    }
    const FVector P = TestPawn->GetActorLocation();
    const FVector F = TestCamera->GetFocusLocation();
    const FVector C = TestCamera->GetActorLocation();
    const FVector V = PixelProbe->GetComponentLocation();
    FVector2D Screen;
    CastChecked<APlayerController>(TestPawn->GetController())->ProjectWorldLocationToScreen(V, Screen);
    if (bDanMotionStudy)
    {
        MotionCsv += FString::Printf(TEXT("%d,%d,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),
            MotionFrame, MotionFrame / 64, *PixelProbe->GetPoseKey().ToString(), P.X, P.Y,
            C.X, C.Y, PixelProbe->GetStrideDistance(), Screen.X, Screen.Y);
    }
    else { MotionCsv += FString::Printf(TEXT("%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),
        MotionFrame, MotionFrame / 60.0, P.X, P.Y, P.Z, F.X, F.Y, F.Z, C.X, C.Y, C.Z, V.X, V.Y, V.Z, Screen.X, Screen.Y);
    }
    ++MotionFrame;
    bCapturePending = false;
}

namespace
{
    const TCHAR* LightingNames[] = {TEXT("00-unlit"), TEXT("01-lit-baseline"), TEXT("02-front"),
        TEXT("03-left"), TEXT("04-right"), TEXT("05-back"), TEXT("06-occluded"), TEXT("07-revealed")};
}

void AGyeolQuarterViewLabMode::TickLightingProbe(float DeltaSeconds)
{
    if (bCapturePending)
    {
        if (FPlatformTime::Seconds() - CaptureRequestedAt > 30.0)
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Lighting capture timed out."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return;
    }
    if (LightingStage >= UE_ARRAY_COUNT(LightingNames))
    {
        const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QuarterViewLab"), CaptureTag, TEXT("lighting.csv"));
        const bool bSaved = FFileHelper::SaveStringToFile(LightingCsv, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogQuarterViewLab, Display, TEXT("LIGHTING %s: captures complete; run the matching pixel/lighting checker."),
            bSaved ? TEXT("CAPTURE PASS") : TEXT("CAPTURE FAIL"));
        FPlatformMisc::RequestExitWithStatus(false, bSaved ? 0 : 1);
        return;
    }
    if (!bLightingPrepared)
    {
        const FRotationMatrix Basis(TestCamera->ViewRotation);
        const FVector Front = -Basis.GetUnitAxis(EAxis::X);
        const FVector Right = Basis.GetUnitAxis(EAxis::Y);
        const FVector Up = Basis.GetUnitAxis(EAxis::Z);
        const FVector Center = TestPawn->GetActorLocation() - FVector(0.0, 0.0, 85.0)
            + Up * (PixelProbe->GetLabSprite()->WorldExtent.Y * 0.5);
        SceneSun->GetLightComponent()->SetIntensity(0.5f);
        UPointLightComponent* Point = CastChecked<UPointLightComponent>(SceneLamp->GetLightComponent());
        Point->SetLightColor(FLinearColor::White);
        Point->SetCastShadows(false);
        Point->SetAttenuationRadius(750.0f);
        Point->SetIntensity(LightingStage >= 2 && LightingStage <= 5 ? 2500.0f : 0.0f);
        FVector Offset = Front * 200.0;
        if (LightingStage == 3) { Offset = Front * 80.0 - Right * 180.0; }
        if (LightingStage == 4) { Offset = Front * 80.0 + Right * 180.0; }
        if (LightingStage == 5) { Offset = -Front * 200.0; }
        SceneLamp->SetActorLocation(Center + Offset);
        const bool bLit = LightingStage >= 1 && LightingStage <= 5;
        if (!PixelProbe->SetLit(bLit))
        {
            UE_LOG(LogQuarterViewLab, Error, TEXT("Lighting material unavailable."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        LightingOccluder->SetActorHiddenInGame(LightingStage != 6);
        LightingFrames = 0;
        LightingTime = 0.0;
        bLightingPrepared = true;
        return;
    }
    LightingTime += DeltaSeconds;
    ++LightingFrames;
    // Keep shader startup and light/material changes out of the sampled frame.
    if (LightingFrames >= 60 && LightingTime >= 0.75)
    {
        if (LightingStage == 1 && FParse::Param(FCommandLine::Get(), TEXT("GyeolTraceNormalGPU")))
        {
            // One local BasePass only; no upload or automatic desktop window.
            auto& Console = IConsoleManager::Get();
            Console.FindConsoleVariable(TEXT("r.DumpGPU.Upload"))->Set(0, ECVF_SetByCode);
            Console.FindConsoleVariable(TEXT("r.DumpGPU.Explore"))->Set(0, ECVF_SetByCode);
            Console.FindConsoleVariable(TEXT("r.DumpGPU.Buffer"))->Set(0, ECVF_SetByCode);
            Console.FindConsoleVariable(TEXT("r.DumpGPU.Root"))->Set(TEXT("BasePass*"), ECVF_SetByCode);
            Console.FindConsoleVariable(TEXT("r.DumpGPU.FrameCount"))->Set(1, ECVF_SetByCode);
            Console.FindConsoleVariable(TEXT("r.DumpGPU.Directory"))->Set(*FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QuarterViewLab"), CaptureTag, TEXT("gpu"))), ECVF_SetByCode);
            GEngine->Exec(GetWorld(), TEXT("DumpGPU"));
        }
        bCapturePending = true;
        CaptureRequestedAt = FPlatformTime::Seconds();
        Capture(LightingNames[LightingStage]);
    }
}

void AGyeolQuarterViewLabMode::OnLightingCaptureProcessed()
{
    if (!bCapturePending || !IsValid(PixelProbe))
    {
        return;
    }
    FVector2D Screen;
    CastChecked<APlayerController>(TestPawn->GetController())->ProjectWorldLocationToScreen(PixelProbe->GetComponentLocation(), Screen);
    const UGyeolProbeSprite* Sprite = PixelProbe->GetLabSprite();
    const FVector L = SceneLamp->GetActorLocation();
    LightingCsv += FString::Printf(TEXT("%d,%s,%.9f,%.9f,%.9f,%.9f,%d,%d,%d,%d,%.9f,%.9f,%.9f,%.9f,%d,%d\n"),
        LightingStage, LightingNames[LightingStage], Screen.X, Screen.Y, Sprite->WorldExtent.X, Sprite->WorldExtent.Y,
        Sprite->SourceRect.Min.X, Sprite->SourceRect.Min.Y, Sprite->SourceRect.Max.X, Sprite->SourceRect.Max.Y,
        L.X, L.Y, L.Z, SceneLamp->GetLightComponent()->Intensity, LightingStage >= 1 && LightingStage <= 5, LightingStage == 6);
    ++LightingStage;
    bCapturePending = false;
    bLightingPrepared = false;
}

void AGyeolQuarterViewLabMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CaptureProcessedHandle.IsValid())
    {
        FScreenshotRequest::OnScreenshotRequestProcessed().Remove(CaptureProcessedHandle);
    }
    if (bMotionProbe)
    {
        FApp::SetUseFixedTimeStep(bPreviousFixedTimeStep);
        FApp::SetFixedDeltaTime(PreviousFixedDelta);
    }
    Super::EndPlay(EndPlayReason);
}
