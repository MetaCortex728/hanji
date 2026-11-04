// SPDX-License-Identifier: Apache-2.0
#include "HanjiSpriteComponent.h"

#include "Components/DecalComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HanjiAssetLibrary.h"
#include "HanjiClipTable.h"
#include "HanjiCoreIncludes.h"
#include "HanjiSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

struct FHanjiPlayRequest
{
	hanji::PlayOptions Options;
};

UHanjiSpriteComponent::UHanjiSpriteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bReceivesDecals = false;
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

UHanjiSpriteComponent::~UHanjiSpriteComponent() = default;

void UHanjiSpriteComponent::OnRegister()
{
	if (!GetStaticMesh())
	{
		SetStaticMesh(LoadObject<UStaticMesh>(nullptr, UHanjiAssetLibrary::QuadMeshPath));
	}
	Super::OnRegister();
	if (UWorld* World = GetWorld())
	{
		if (UHanjiSubsystem* Subsystem = World->GetSubsystem<UHanjiSubsystem>()) Subsystem->Register(this);
	}
	bHasLastLocation = false;
	Player.Reset();
	if (EnsurePlayer() && !InitialState.IsNone()) PlayState(InitialState);
	UpdateVisual(LastView, true);
}

void UHanjiSpriteComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UHanjiSubsystem* Subsystem = World->GetSubsystem<UHanjiSubsystem>()) Subsystem->Unregister(this);
	}
	if (bApplyHitStopToOwner && Player && Player->hit_stopped())
	{
		if (AActor* Owner = GetOwner()) Owner->CustomTimeDilation = 1.0f;
	}
	if (Shadow)
	{
		Shadow->DestroyComponent();
		Shadow = nullptr;
	}
	Super::OnUnregister();
}

#if WITH_EDITOR
void UHanjiSpriteComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	Player.Reset();
	Material = nullptr;
	if (EnsurePlayer() && !InitialState.IsNone()) PlayState(InitialState);
	UpdateVisual(LastView, true);
}
#endif

bool UHanjiSpriteComponent::EnsurePlayer()
{
	if (Player) return true;
	if (!ClipTable || !ClipTable->GetTable()) return false;
	Player = MakeShared<hanji::Player>(*ClipTable->GetTable());
	StatePolicy = nullptr;
	CurrentState = NAME_None;
	LastFrame = LastHeading = INDEX_NONE;
	if (!Material)
	{
		UMaterialInterface* Base = SpriteMaterial ? SpriteMaterial.Get() : LoadObject<UMaterialInterface>(nullptr, UHanjiAssetLibrary::LitMaterialPath);
		if (Base)
		{
			Material = UMaterialInstanceDynamic::Create(Base, this);
			if (ClipTable->Atlas) Material->SetTextureParameterValue(TEXT("Atlas"), ClipTable->Atlas);
			SetMaterial(0, Material);
		}
	}
	return true;
}

bool UHanjiSpriteComponent::PlayWith(FName Action, const FHanjiPlayRequest& Request)
{
	if (!EnsurePlayer()) return false;
	if (!Player->play(HanjiConvert::Std(Action.ToString()), Request.Options)) return false;
	// The first frame begins now; its events (a whoosh, a first footstep) must not wait for the next advance.
	BroadcastEvents();
	return true;
}

bool UHanjiSpriteComponent::PlayState(FName State)
{
	if (!EnsurePlayer()) return false;
	const hanji::PolicyTable* Policies = ClipTable->GetPolicies();
	const hanji::StatePolicy* Policy = Policies ? Policies->find(HanjiConvert::Std(State.ToString())) : nullptr;
	FHanjiPlayRequest Request;
	FName Action = State;
	if (Policy)
	{
		Request.Options = hanji::play_options(*Policy, Player->phase());
		Action = FName(HanjiConvert::String(Policy->action));
	}
	if (!PlayWith(Action, Request)) return false;
	StatePolicy = Policy;
	CurrentState = State;
	return true;
}

bool UHanjiSpriteComponent::PlayAction(FName Action, float Speed, bool bRestart)
{
	FHanjiPlayRequest Request;
	Request.Options.speed = Speed;
	Request.Options.restart_if_same = bRestart;
	if (!PlayWith(Action, Request)) return false;
	StatePolicy = nullptr;
	CurrentState = Action;
	return true;
}

bool UHanjiSpriteComponent::PlayTimed(FName Action, float Startup, float Active, float Recovery)
{
	FHanjiPlayRequest Request;
	Request.Options.timing = hanji::GameplayTiming{Startup, Active, Recovery};
	Request.Options.restart_if_same = true;
	if (!PlayWith(Action, Request)) return false;
	StatePolicy = nullptr;
	CurrentState = Action;
	return true;
}

void UHanjiSpriteComponent::SetFacingDirection(FVector WorldDirection)
{
	LastFacing = WorldDirection;
}

void UHanjiSpriteComponent::HitStop(float Duration, float TimeScale)
{
	if (!EnsurePlayer()) return;
	hanji::HitStopPolicy Policy;
	Policy.duration_s = Duration;
	Policy.time_scale = TimeScale;
	Player->hit_stop(Policy);
}

bool UHanjiSpriteComponent::ApplyStateHitStop()
{
	if (!Player || !StatePolicy || !StatePolicy->hit_stop) return false;
	Player->hit_stop(*StatePolicy->hit_stop);
	return true;
}

FName UHanjiSpriteComponent::GetCurrentAction() const
{
	return Player && Player->action() ? FName(HanjiConvert::String(Player->action()->name)) : NAME_None;
}

int32 UHanjiSpriteComponent::GetFrame() const { return Player ? Player->frame() : INDEX_NONE; }
FName UHanjiSpriteComponent::GetHeading() const
{
	return Player ? FName(HanjiConvert::String(std::string(hanji::heading_name(Player->heading())))) : NAME_None;
}
bool UHanjiSpriteComponent::IsInActiveWindow() const { return Player && Player->in_active_window(); }
bool UHanjiSpriteComponent::IsFinished() const { return Player && Player->finished(); }
bool UHanjiSpriteComponent::IsHitStopped() const { return Player && Player->hit_stopped(); }
float UHanjiSpriteComponent::GetPhase() const { return Player ? static_cast<float>(Player->phase()) : 0.0f; }

float UHanjiSpriteComponent::PixelSize() const
{
	if (CentimetresPerPixel > 0.0f) return CentimetresPerPixel;
	const float FromTable = ClipTable ? ClipTable->GetCentimetresPerPixel() : 0.0f;
	return FromTable > 0.0f ? FromTable : 1.0f;
}

FVector UHanjiSpriteComponent::GetPixelWorldLocation(FVector2D Pixel) const
{
	if (!Player || !Player->action()) return GetComponentLocation();
	const hanji::IntRect& Source = Player->current().source;
	const double W = FMath::Max(Source.w, 1), H = FMath::Max(Source.h, 1);
	return GetComponentTransform().TransformPosition(FVector(0.0, Pixel.X / W - 0.5, 1.0 - Pixel.Y / H));
}

TArray<FHanjiBox> UHanjiSpriteComponent::GetActiveBoxes() const
{
	TArray<FHanjiBox> Boxes;
	if (!Player || !Player->action()) return Boxes;
	const float Size = PixelSize();
	const FVector Scale = GetComponentScale();
	const double Stretch = Scale.Y > 0 ? Scale.Z / Scale.Y : 1.0;  // the quad's height scale
	for (const hanji::Hitbox& B : Player->current().boxes)
	{
		FHanjiBox Box;
		Box.Name = FName(HanjiConvert::String(B.name));
		Box.bAttack = B.kind == hanji::BoxKind::Attack;
		Box.Center = GetPixelWorldLocation(FVector2D(B.box.x + B.box.w * 0.5, B.box.y + B.box.h * 0.5));
		Box.HalfSize = FVector2D(B.box.w * 0.5 * Size, B.box.h * 0.5 * Size * Stretch);
		Boxes.Add(Box);
	}
	return Boxes;
}

void UHanjiSpriteComponent::HanjiAdvance(float DeltaSeconds, const FRotator& ViewRotation)
{
	if (!EnsurePlayer()) return;

	// Travel since the last advance, on the ground plane, from the anchor (the attach point).
	const FVector Anchor = AnchorLocation();
	const double Moved = bHasLastLocation ? FVector::Dist2D(Anchor, LastLocation) : 0.0;
	LastLocation = Anchor;
	bHasLastLocation = true;

	// Heading from the facing direction, unless the state locks it.
	const bool bLocked = StatePolicy && StatePolicy->lock_facing && !Player->finished();
	if (!bLocked && !LastFacing.IsNearlyZero())
	{
		Player->set_heading(hanji::select_heading(HanjiConvert::Vec(LastFacing), HanjiConvert::View(ViewRotation),
			Player->heading(), HeadingHysteresisDegrees));
	}

	const bool bWasStopped = Player->hit_stopped();
	Player->advance(DeltaSeconds, Moved);
	if (bApplyHitStopToOwner)
	{
		if (AActor* Owner = GetOwner())
		{
			if (Player->hit_stopped()) Owner->CustomTimeDilation = static_cast<float>(FMath::Max(Player->dilation(), 1e-4));
			else if (bWasStopped) Owner->CustomTimeDilation = 1.0f;
		}
	}
	BroadcastEvents();
	UpdateVisual(ViewRotation, false);
}

void UHanjiSpriteComponent::BroadcastEvents()
{
	if (!Player || !Player->action()) return;
	const FName Action(HanjiConvert::String(Player->action()->name));
	for (const hanji::FiredEvent& E : Player->events())
	{
		OnFrameEvent.Broadcast(FName(HanjiConvert::String(E.name)), Action, E.frame);
	}
	if (Player->finished() && FinishedPlayId != Player->play_id())
	{
		FinishedPlayId = Player->play_id();
		OnActionFinished.Broadcast(Action);
	}
}

FVector UHanjiSpriteComponent::AnchorLocation() const
{
	if (const USceneComponent* Parent = GetAttachParent())
	{
		return Parent->GetSocketTransform(GetAttachSocketName()).TransformPosition(AnchorOffset);
	}
	return AnchorOffset;
}

void UHanjiSpriteComponent::UpdateVisual(const FRotator& ViewRotation, bool bForce)
{
	if (!Player || !Player->action() || !ClipTable || !ClipTable->GetTable()) return;
	const hanji::ClipTable& Table = *ClipTable->GetTable();
	const hanji::Frame& Frame = Player->current();

	// The material: which cell of the atlas, and mirrored or not.
	const int32 FrameIndex = Player->frame();
	const int32 HeadingIndex = hanji::heading_index(Player->heading());
	if (bForce || FrameIndex != LastFrame || HeadingIndex != LastHeading)
	{
		LastFrame = FrameIndex;
		LastHeading = HeadingIndex;
		const double IW = FMath::Max(Table.image_w, 1), IH = FMath::Max(Table.image_h, 1);
		LastUV = FVector4(Frame.source.x / IW, Frame.source.y / IH, Frame.source.w / IW, Frame.source.h / IH);
		bLastMirror = Frame.mirror_x;
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("FrameUV"), FLinearColor(LastUV.X, LastUV.Y, LastUV.Z, LastUV.W));
			Material->SetScalarParameterValue(TEXT("Mirror"), bLastMirror ? 1.0f : 0.0f);
		}
	}

	// The quad: standing at the tilt, scaled to the cell, its ground pivot on the anchor.
	LastView = ViewRotation;
	const hanji::ViewRotation View = HanjiConvert::View(ViewRotation);
	const hanji::SpriteQuad Quad = hanji::sprite_quad(View, TiltDegrees);
	const double Size = PixelSize();
	const double W = Frame.source.w * Size;
	const double H = Frame.source.h * Size * Quad.height_scale;
	const FVector Right = HanjiConvert::Vector(Quad.right);
	const FVector Up = HanjiConvert::Vector(Quad.up);

	FVector Anchor = AnchorLocation();
	if (bSnapToPixelGrid)
	{
		Anchor = HanjiConvert::Vector(hanji::snap_ground_anchor(HanjiConvert::Vec(Anchor), {}, View, Size));
	}
	const double PivotX = Frame.mirror_x ? Frame.source.w - Table.pivot.x : Table.pivot.x;
	const FVector PivotOnQuad = Right * ((PivotX / FMath::Max(Frame.source.w, 1) - 0.5) * W) + Up * ((1.0 - Table.pivot.y / static_cast<double>(FMath::Max(Frame.source.h, 1))) * H);
	const FQuat Rotation = FRotationMatrix::MakeFromYZ(Right, Up).ToQuat();
	SetWorldTransform(FTransform(Rotation, Anchor - PivotOnQuad, FVector(1.0, W, H)));
	UpdateShadowAndAttachments(AnchorLocation(), ViewRotation);
}

void UHanjiSpriteComponent::AttachToPixel(USceneComponent* Component, FVector2D Pixel, float DepthBias)
{
	if (!Component) return;
	DetachFromPixel(Component);
	Component->SetUsingAbsoluteLocation(true);
	Attachments.Add({Component, Pixel, DepthBias});
	UpdateShadowAndAttachments(AnchorLocation(), LastView);
}

void UHanjiSpriteComponent::DetachFromPixel(USceneComponent* Component)
{
	Attachments.RemoveAll([Component](const FPixelAttachment& A) { return !A.Component.IsValid() || A.Component.Get() == Component; });
}

void UHanjiSpriteComponent::UpdateShadowAndAttachments(const FVector& Anchor, const FRotator& ViewRotation)
{
	if (!Player || !Player->action()) return;
	const hanji::Frame& Frame = Player->current();

	// Effects on pixels: the pixel as drawn, mirrored with the frame, then nudged toward the camera.
	const FVector Toward = -ViewRotation.Vector();
	for (int32 I = Attachments.Num() - 1; I >= 0; --I)
	{
		USceneComponent* C = Attachments[I].Component.Get();
		if (!C) { Attachments.RemoveAtSwap(I); continue; }
		FVector2D Pixel = Attachments[I].Pixel;
		if (Frame.mirror_x) Pixel.X = Frame.source.w - Pixel.X;
		C->SetWorldLocation(GetPixelWorldLocation(Pixel) + Toward * Attachments[I].DepthBias);
	}

	// The blob shadow: a decal projected down onto whatever is under the pivot. Sprites ignore decals.
	if (bBlobShadow && !Shadow && GetOwner() && GetWorld())
	{
		Shadow = NewObject<UDecalComponent>(GetOwner(), NAME_None, RF_Transient);
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, UHanjiAssetLibrary::BlobMaterialPath))
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Shadow);
			M->SetScalarParameterValue(TEXT("Opacity"), BlobShadowOpacity);
			Shadow->SetDecalMaterial(M);
		}
		Shadow->SetUsingAbsoluteLocation(true);
		Shadow->SetUsingAbsoluteRotation(true);
		Shadow->SetUsingAbsoluteScale(true);
		Shadow->SetupAttachment(GetAttachParent() ? GetAttachParent() : this);
		Shadow->RegisterComponent();
	}
	if (Shadow)
	{
		Shadow->SetVisibility(bBlobShadow);
		const double Radius = BlobShadowRadius > 0.0f ? BlobShadowRadius : Frame.source.w * PixelSize() / 3.0;
		// Decals project along their X axis: point it down; 30 cm deep reaches floors a little below the pivot.
		Shadow->DecalSize = FVector(30.0, Radius, Radius);
		Shadow->SetWorldLocationAndRotation(Anchor, FRotator(-90.0, 0.0, 0.0));
		Shadow->MarkRenderStateDirty();
	}
}
