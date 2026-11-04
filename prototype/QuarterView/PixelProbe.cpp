#include "PixelProbe.h"

#include "QuarterViewCamera.h"
#include "QuarterViewMath.h"
#include "PoseAtlas.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "ImageCore.h"
#include "ImageUtils.h"

bool UGyeolProbeSprite::InitializeProbe(double UnitsPerPixel)
{
    constexpr int32 Width = 32;
    constexpr int32 Height = 48;
    TArray<FColor> Pixels;
    Pixels.Init(FColor::Transparent, Width * Height);
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const bool bVisible = (X >= 12 && X <= 21 && Y <= 10)
                || (X >= 8 && X <= 24 && Y >= 11 && Y <= 29)
                || (X >= 4 && X <= 7 && Y >= 14 && Y <= 28)
                || (X >= 25 && X <= 29 && Y >= 15 && Y <= 32)
                || (X >= 9 && X <= 15 && Y >= 30 && Y <= 45)
                || (X >= 20 && X <= 26 && Y >= 30);
            if (bVisible)
            {
                Pixels[Y * Width + X] = (X + 2 * Y) % 5 < 2
                    ? FColor(20, 235, 245) : FColor(235, 40, 230);
            }
        }
    }
    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8,
        NAME_None, TConstArrayView64<uint8>(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)));
    return InitializeTexture(Texture, FIntRect(0, 0, Width, Height), UnitsPerPixel, false);
}

bool UGyeolProbeSprite::InitializeImage(const FString& Path, double WorldHeight)
{
    FImage Image;
    if (!FImageUtils::LoadImage(*Path, Image) || Image.NumSlices != 1 || WorldHeight <= 0.0)
    {
        return false;
    }
    Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    const TArrayView64<FColor> Pixels = Image.AsBGRA8();
    FIntRect Rect(Image.SizeX, Image.SizeY, 0, 0);
    for (int32 Y = 0; Y < Image.SizeY; ++Y)
    {
        for (int32 X = 0; X < Image.SizeX; ++X)
        {
            if (Pixels[Y * Image.SizeX + X].A > 85)
            {
                Rect.Min.X = FMath::Min(Rect.Min.X, X);
                Rect.Min.Y = FMath::Min(Rect.Min.Y, Y);
                Rect.Max.X = FMath::Max(Rect.Max.X, X + 1);
                Rect.Max.Y = FMath::Max(Rect.Max.Y, Y + 1);
            }
        }
    }
    if (Rect.Width() <= 0 || Rect.Height() <= 0)
    {
        return false;
    }
    return InitializeTexture(UTexture2D::CreateTransientFromImage(&Image), Rect, WorldHeight / Rect.Height(), true);
}

bool UGyeolProbeSprite::InitializeAtlasFrame(UTexture2D* Texture, const FIntRect& Rect, double UnitsPerPixel, const FVector2D& Pivot)
{
    return InitializeTexture(Texture, Rect, UnitsPerPixel, true, &Pivot);
}

bool UGyeolProbeSprite::InitializeTexture(UTexture2D* Texture, const FIntRect& Rect, double UnitsPerSourcePixel, bool bLit, const FVector2D* Pivot)
{
    DefaultMaterial = LoadObject<UMaterialInterface>(nullptr,
        bLit ? TEXT("/Paper2D/MaskedLitSpriteMaterial.MaskedLitSpriteMaterial")
            : TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
    if (!Texture || !DefaultMaterial || UnitsPerSourcePixel <= 0.0)
    {
        return false;
    }
    const bool bNeedsUpdate = Texture->Filter != TF_Nearest || Texture->AddressX != TA_Clamp
        || Texture->AddressY != TA_Clamp || !Texture->NeverStream || !Texture->SRGB || !Texture->GetResource();
    Texture->Filter = TF_Nearest;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;
    Texture->NeverStream = true;
    Texture->SRGB = true;
    if (bNeedsUpdate) { Texture->UpdateResource(); }
    BakedSourceTexture = Texture;
    BakedSourceUV = FVector2D(Rect.Min);
    BakedSourceDimension = FVector2D(Rect.Size());
    SourceRect = Rect;
    WorldExtent = FVector2D(Rect.Size()) * UnitsPerSourcePixel;
    PixelsPerUnrealUnit = 1.0 / UnitsPerSourcePixel;
    SpriteCollisionDomain = ESpriteCollisionMode::None;
    AlternateMaterialSplitIndex = INDEX_NONE;
    const double L = Pivot ? Pivot->X * UnitsPerSourcePixel : WorldExtent.X * 0.5;
    const double R = L - WorldExtent.X;
    const double T = Pivot ? Pivot->Y * UnitsPerSourcePixel : WorldExtent.Y;
    const double B = T - WorldExtent.Y;
    const double U0 = double(Rect.Min.X) / Texture->GetSizeX();
    const double U1 = double(Rect.Max.X) / Texture->GetSizeX();
    const double V0 = double(Rect.Min.Y) / Texture->GetSizeY();
    const double V1 = double(Rect.Max.Y) / Texture->GetSizeY();
    // Paper2D stores local X/Z positions followed by texture U/V; use the explicit or bottom-center pivot.
    BakedRenderData = {FVector4(L, T, U0, V0), FVector4(R, T, U1, V0), FVector4(R, B, U1, V1),
        FVector4(L, T, U0, V0), FVector4(R, B, U1, V1), FVector4(L, B, U0, V1)};
    return true;
}

UGyeolPixelProbeComponent::UGyeolPixelProbeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCastShadow(false);
    bReceivesDecals = false;
}

bool UGyeolPixelProbeComponent::InitializeProbe(AGyeolQuarterViewCamera* InCamera, bool bInSnap, double Scale)
{
    Camera = InCamera;
    bSnap = bInSnap;
    if (!IsValid(Camera) || Scale <= 0.0)
    {
        return false;
    }
    UGyeolProbeSprite* Sprite = NewObject<UGyeolProbeSprite>(this);
    if (!Sprite->InitializeProbe(Camera->ViewWidth / Camera->InternalWidth))
    {
        return false;
    }
    SetSprite(Sprite);
    SetWorldScale3D(FVector(Scale));
    AddTickPrerequisiteActor(Camera);
    return true;
}

bool UGyeolPixelProbeComponent::InitializeImage(AGyeolQuarterViewCamera* InCamera, bool bInSnap,
    const FString& Path, double PixelHeight)
{
    Camera = InCamera;
    bSnap = bInSnap;
    if (!IsValid(Camera) || PixelHeight <= 0.0)
    {
        return false;
    }
    UGyeolProbeSprite* Sprite = NewObject<UGyeolProbeSprite>(this);
    if (!Sprite->InitializeImage(Path, PixelHeight * Camera->ViewWidth / Camera->InternalWidth))
    {
        return false;
    }
    SetSprite(Sprite);
    SetWorldScale3D(FVector::OneVector);
    AddTickPrerequisiteActor(Camera);
    return true;
}

bool UGyeolPixelProbeComponent::SetLit(bool bLit)
{
    if (bLit && NormalMaterial) { SetMaterial(0, NormalMaterial); return true; }
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
        bLit ? TEXT("/Paper2D/MaskedLitSpriteMaterial.MaskedLitSpriteMaterial")
            : TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"));
    if (!Material)
    {
        return false;
    }
    SetMaterial(0, Material);
    return true;
}

bool UGyeolPixelProbeComponent::InitializeAtlas(AGyeolQuarterViewCamera* InCamera, bool bInSnap, const FString& Directory)
{
    Camera = InCamera;
    bSnap = bInSnap;
    FString Json;
    GyeolPose::FAtlas Atlas;
    FImage Image;
    if (!IsValid(Camera) || !FFileHelper::LoadFileToString(Json, *FPaths::Combine(Directory, TEXT("pose-proxy.json")))
        || !GyeolPose::Parse(Json, Atlas) || !FImageUtils::LoadImage(*FPaths::Combine(Directory, TEXT("pose-proxy.png")), Image)
        || Image.NumSlices != 1 || Image.SizeX != Atlas.Size.X || Image.SizeY != Atlas.Size.Y) { return false; }
    Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    UTexture2D* Texture = UTexture2D::CreateTransientFromImage(&Image);
    AtlasViews = Atlas.ViewCount;
    ContactAnchors = Atlas.ContactAnchors;
    AtlasPivot = Atlas.Pivot;
    AtlasUnitsPerPixel = Camera->ViewWidth / Camera->InternalWidth;
    bHasWalking = Atlas.bWalking;
    bHasAttackTransitions = Atlas.bAttackTransitions;
    bHasMovementEntry = Atlas.bMovementEntry;
    bHasStopTransitions = Atlas.bStopTransitions;
    bHasRepresentativeStrike = Atlas.bRepresentativeStrike;
    bHasRepresentativeStrikeEntry = Atlas.bRepresentativeStrikeEntry;
    NativeStrike = Atlas.NativeStrike;
    NativeKick = Atlas.NativeKick;
    bHasPhasedDash = Atlas.bPhasedDash;
    DashPlayback.Reset();
    StopTransition.Reset();
    Stride.PoseCount = Atlas.WalkPoseCount;
    WalkCycleCm = 180.0;
    if (bHasWalking)
    {
        const FString ContractPath = FPaths::Combine(Directory, TEXT("pose-contract.json"));
        if (FPaths::FileExists(ContractPath))
        {
            if (!FFileHelper::LoadFileToString(Json, *ContractPath)
                || !GyeolPose::ParseLocomotion(Json, Stride.PoseCount, WalkCycleCm,
                    Atlas.bLocomotionOnly || NativeStrike.IsValid())) { return false; }
        }
        else if (Stride.PoseCount != 4 || Atlas.bLocomotionOnly || NativeStrike.IsValid()) { return false; }
    }
    int32 OpaqueBottom = Atlas.Pivot.Y;
    const auto Pixels = Image.AsBGRA8();
    for (const auto& Frame : Atlas.Frames)
    {
        for (int32 Y = 0; Y < Frame.Rect.Height(); ++Y)
        {
            for (int32 X = 0; X < Frame.Rect.Width(); ++X)
            {
                if (Pixels[(Frame.Rect.Min.Y + Y) * Image.SizeX + Frame.Rect.Min.X + X].A > 85)
                { OpaqueBottom = FMath::Max(OpaqueBottom, Y + 1); }
            }
        }
        auto* Sprite = NewObject<UGyeolProbeSprite>(this);
        if (!Sprite->InitializeAtlasFrame(Texture, Frame.Rect, Camera->ViewWidth / Camera->InternalWidth, FVector2D(Atlas.Pivot)))
        { PoseSprites.Reset(); return false; }
        PoseSprites.Add(Frame.Name, Sprite);
    }
    // One depth-only offset for all poses avoids below-pivot foot clipping and pose-dependent popping.
    // This flat-floor lab workaround is not per-pixel character depth or a stair/occlusion solution.
    AtlasDepthOffset = GyeolPose::GroundClearanceOffset(OpaqueBottom - Atlas.Pivot.Y,
        Camera->ViewWidth / Camera->InternalWidth, Camera->ViewRotation);
    SetWorldScale3D(FVector::OneVector);
    AddTickPrerequisiteActor(Camera);
    return SetPose(TEXT("idle"));
}

bool UGyeolPixelProbeComponent::GetContactPoint(FName Pose, const FVector& Direction, FVector& Out) const
{
    if (!IsValid(Camera) || !GetOwner()) { return false; }
    const int32 Sector = GyeolPose::ScreenDirection(Direction, Camera->ViewRotation, FacingSector);
    const auto* Point = ContactAnchors.Find(GyeolPose::PoseKey(Pose, Sector, AtlasViews));
    if (!Point) { return false; }
    FVector Foot = GetOwner()->GetActorLocation() - FVector(0, 0, 85);
    if (bSnap)
    {
        Foot = GyeolQuarterView::SnapGroundAnchor(Foot, Camera->GetActorLocation(), Camera->ViewRotation,
            Camera->GetRenderUnitsPerPixel());
    }
    Out = Foot + AtlasDepthOffset + GyeolPose::ContactOffset(*Point, AtlasPivot, AtlasUnitsPerPixel, Camera->ViewRotation);
    return true;
}

FName UGyeolPixelProbeComponent::ResolveDashPose(FName Fallback, FVector& Direction,
    bool bDash, bool bNeutral, uint64 Instance, double Elapsed, double GameTime, double DashSeconds)
{
    if (!bHasPhasedDash) { return Fallback; }
    const bool bWasRecovering = DashPlayback.bRecovering;
    const int32 Tick = DashPlayback.Sample(bDash, bNeutral, Instance, Elapsed, GameTime,
        DashSeconds, DisplayedWalkPhase);
    if (Tick == INDEX_NONE) { return Fallback; }
    if (bDash) { DashDirection = Direction; }
    if (DashPlayback.bRecovering)
    {
        NeutralDirection = Direction;
        Direction = DashDirection;
        if (!bWasRecovering)
        {
            ResetLocomotion(false);
            // Baseline after the final dash displacement; only later neutral motion resumes run.
            if (GetOwner()) { Stride.Sample(GetOwner()->GetActorLocation() - FVector(0, 0, 85), true); }
        }
    }
    return DashPlayback.Pose(Tick);
}

bool UGyeolPixelProbeComponent::SetPose(FName Name, const FVector& Direction)
{
    if (!PoseSprites.Contains(GyeolPose::PoseKey(Name, 0, AtlasViews))) { return false; }
    RequestedPose = Name;
    RequestedDirection = Direction;
    // Neutral animation is resolved after movement; action poses stay immediate.
    if (bHasWalking && Name == FName(TEXT("idle")) && !CurrentPoseKey.IsNone()) { return true; }
    if (Name != FName(TEXT("idle")) && !DashPlayback.bRecovering) { ResetLocomotion(false); }
    return DisplayPose(Name, Direction);
}

bool UGyeolPixelProbeComponent::DisplayPose(FName Name, const FVector& Direction)
{
    const int32 Sector = IsValid(Camera) ? GyeolPose::ScreenDirection(Direction, Camera->ViewRotation, FacingSector) : 0;
    const FName Key = GyeolPose::PoseKey(Name, Sector, AtlasViews);
    const auto* Sprite = PoseSprites.Find(Key);
    if (!Sprite) { return false; }
    if (Key != CurrentPoseKey) { SetSprite(*Sprite); }
    FacingSector = Sector;
    CurrentPoseKey = Key;
    SeenPoses.Add(Name);
    SeenPoseKeys.Add(Key);
    return true;
}

UGyeolProbeSprite* UGyeolPixelProbeComponent::GetLabSprite()
{
    return Cast<UGyeolProbeSprite>(GetSprite());
}

void UGyeolPixelProbeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsValid(Camera) || !GetOwner())
    {
        return;
    }
    FVector Foot = GetOwner()->GetActorLocation() - FVector(0.0, 0.0, 85.0);
    if (bHasWalking)
    {
        Stride.CycleCm = WalkCycleCm;
        if (DashPlayback.bRecovering)
        {
            // Early movement joins the current authored run phase, not the completed exit pose.
            Stride.Distance = ((DashPlayback.Bucket * 6 + DashPlayback.RecoveryTick) % 48) * WalkCycleCm / 48.0;
        }
        const int32 Walk = Stride.Sample(Foot, RequestedPose == FName(TEXT("idle")) || DashPlayback.bRecovering);
        if (DashPlayback.bRecovering && Walk != INDEX_NONE)
        {
            DashPlayback.Reset();
            RequestedPose = TEXT("idle");
            RequestedDirection = NeutralDirection;
        }
        DisplayedWalkPhase = Walk;
        const FName Pose = bHasStopTransitions && RequestedPose == FName(TEXT("idle"))
            ? StopTransition.Sample(Walk, DeltaTime)
            : Walk == INDEX_NONE ? RequestedPose : FName(*FString::Printf(TEXT("walk%d"), Walk));
        DisplayPose(Pose, RequestedDirection);
    }
    if (bSnap)
    {
        Foot = GyeolQuarterView::SnapGroundAnchor(Foot, Camera->GetActorLocation(), Camera->ViewRotation,
            Camera->GetRenderUnitsPerPixel());
    }
    SetWorldLocationAndRotation(Foot + AtlasDepthOffset, GyeolQuarterView::SpriteFacingRotation(Camera->ViewRotation));
}
