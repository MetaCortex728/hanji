#include "ContactShadow.h"
#include "../QuarterView/PixelProbe.h"
#include "../QuarterView/QuarterViewCamera.h"
#include "../QuarterView/QuarterViewMath.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

UGyeolContactShadowComponent::UGyeolContactShadowComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCastShadow(false);
    SetAbsolute(true, true, true);
    bReceivesDecals = false;
}

bool UGyeolContactShadowComponent::Initialize(AGyeolQuarterViewCamera* View, double FootOffset, double Diameter)
{
    if (!View || !GetOwner() || !FMath::IsFinite(FootOffset) || !FMath::IsFinite(Diameter) || Diameter <= 0) { return false; }
    constexpr int32 Size = 32;
    TArray<FColor> Pixels;
    Pixels.SetNumUninitialized(Size * Size);
    for (int32 Y = 0; Y < Size; ++Y)
    {
        for (int32 X = 0; X < Size; ++X)
        { Pixels[Y * Size + X] = FColor(18, 24, 23, GyeolContact::Opacity(X, Y, Size)); }
    }
    auto* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, NAME_None,
        TConstArrayView64<uint8>(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor)));
    auto* Frame = NewObject<UGyeolProbeSprite>(this);
    auto* Material = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial"));
    if (!Material || !Frame->InitializeAtlasFrame(Texture, FIntRect(0, 0, Size, Size), Diameter / Size,
        FVector2D(Size * .5, Size * .5))) { return false; }
    Camera = View; OffsetCm = FootOffset; DiameterCm = Diameter;
    SetSprite(Frame); SetMaterial(0, Material);
    // Rotate Paper2D's X/Z quad onto the ground with its front normal facing up.
    SetWorldRotation(FRotationMatrix::MakeFromXZ(FVector::ForwardVector, FVector::RightVector).ToQuat());
    AddTickPrerequisiteActor(GetOwner());
    AddTickPrerequisiteActor(View);
    TickComponent(0, LEVELTICK_All, nullptr);
    return true;
}

FVector UGyeolContactShadowComponent::FootPosition() const
{
    return GetOwner() ? GetOwner()->GetActorLocation() - FVector(0, 0, OffsetCm) : FVector::ZeroVector;
}

void UGyeolContactShadowComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!Camera || !GetOwner()) { return; }
    const FVector Foot = FootPosition();
    FVector Anchor = GyeolQuarterView::SnapGroundAnchor(Foot, Camera->GetActorLocation(),
        Camera->ViewRotation, Camera->GetRenderUnitsPerPixel());
    // A depth-only lift preserves the exact snapped foot projection.
    Anchor -= Camera->ViewRotation.Vector() * 1.5;
    SetWorldLocation(Anchor);
    SetVisibility(!GetOwner()->IsHidden());
}
