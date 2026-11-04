// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "HanjiSpriteComponent.generated.h"

class UHanjiClipTable;
class UMaterialInstanceDynamic;
class UDecalComponent;
namespace hanji { class Player; struct StatePolicy; }

/** A hitbox of the frame shown, in world space on the sprite's quad. */
USTRUCT(BlueprintType)
struct FHanjiBox
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hanji")
	FName Name;

	/** True for an attack box, false for a hurt box. */
	UPROPERTY(BlueprintReadOnly, Category = "Hanji")
	bool bAttack = true;

	/** Centre of the box on the quad. */
	UPROPERTY(BlueprintReadOnly, Category = "Hanji")
	FVector Center = FVector::ZeroVector;

	/** Half size along the quad's right and up axes, in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Hanji")
	FVector2D HalfSize = FVector2D::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FHanjiFrameEvent, FName, Event, FName, Action, int32, Frame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHanjiActionFinished, FName, Action);

/**
 * Plays one character from a Hanji clip table on a quad in the 3D world: the frame from the Hanji clock,
 * the heading from movement on screen, the quad standing at the chosen tilt with its ground pivot on the
 * component's origin. The component never ticks; UHanjiSubsystem advances every sprite once per frame.
 */
UCLASS(ClassGroup = (Hanji), meta = (BlueprintSpawnableComponent))
class HANJI_API UHanjiSpriteComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UHanjiSpriteComponent();
	virtual ~UHanjiSpriteComponent() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hanji")
	TObjectPtr<UHanjiClipTable> ClipTable;

	/** Material with the parameters Atlas, FrameUV and Mirror. Defaults to Hanji's lit sprite material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hanji")
	TObjectPtr<UMaterialInterface> SpriteMaterial;

	/** State (from the clip table's policies) or action played when the component starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	FName InitialState = TEXT("idle");

	/** Lean of the quad back from upright toward the camera, in degrees: 0 stands it upright (tall sprites
	 *  keep upright depth and do not sink into walls); the camera's pitch makes it face the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "0", ClampMax = "89"))
	float TiltDegrees = 0.0f;

	/** Degrees past a sector boundary before the heading changes, so input on a boundary does not flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "0", ClampMax = "20"))
	float HeadingHysteresisDegrees = 3.0f;

	/** Size of one atlas pixel in the world. 0 uses the clip table's metadata (pose-atlas states it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji", meta = (ClampMin = "0"))
	float CentimetresPerPixel = 0.0f;

	/** Where the ground pivot stands, relative to the attach parent (for a character, usually the bottom of
	 *  its capsule). The component's own location is derived from it every frame, so edit this instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	FVector AnchorOffset = FVector::ZeroVector;

	/** Rounds the ground pivot to the camera's pixel lattice (one atlas pixel), keeping feet on the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	bool bSnapToPixelGrid = true;

	/** Projects a soft round shadow onto the ground under the pivot (a decal, so it never covers the sprite),
	 *  independent of the scene's shadow method. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji|Shadow")
	bool bBlobShadow = true;

	/** Radius of the blob shadow in centimetres; 0 uses a third of the cell width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji|Shadow", meta = (ClampMin = "0"))
	float BlobShadowRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji|Shadow", meta = (ClampMin = "0", ClampMax = "1"))
	float BlobShadowOpacity = 0.45f;

	/** During hit-stop, also set the owner's CustomTimeDilation so its movement and timers follow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hanji")
	bool bApplyHitStopToOwner = true;

	/** Raised for every frame event (footstep, hit, whoosh ...) on the frame it begins. */
	UPROPERTY(BlueprintAssignable, Category = "Hanji")
	FHanjiFrameEvent OnFrameEvent;

	/** Raised once when a one-shot action reaches its end. */
	UPROPERTY(BlueprintAssignable, Category = "Hanji")
	FHanjiActionFinished OnActionFinished;

	/** Enters a state from the clip table's policies (or plays the action of that name when there is none). */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	bool PlayState(FName State);

	/** Plays an action at native timing. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	bool PlayAction(FName Action, float Speed = 1.0f, bool bRestart = false);

	/** Plays a one-shot re-timed so its contact frame begins after Startup and its active window lasts Active. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	bool PlayTimed(FName Action, float Startup, float Active, float Recovery);

	/** The direction the character moves or faces in the world; the heading follows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void SetFacingDirection(FVector WorldDirection);

	/** Slows this sprite's clock: TimeScale 0 freezes it for Duration seconds of real time. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void HitStop(float Duration = 0.07f, float TimeScale = 0.0f);

	/** Applies the current state's hit-stop policy. Returns false when the state has none. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	bool ApplyStateHitStop();

	UFUNCTION(BlueprintPure, Category = "Hanji") FName GetCurrentAction() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") FName GetCurrentState() const { return CurrentState; }
	UFUNCTION(BlueprintPure, Category = "Hanji") int32 GetFrame() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") FName GetHeading() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") bool IsInActiveWindow() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") bool IsFinished() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") bool IsHitStopped() const;
	UFUNCTION(BlueprintPure, Category = "Hanji") float GetPhase() const;

	/** Hitboxes of the frame shown, placed on the quad in world space. */
	UFUNCTION(BlueprintPure, Category = "Hanji")
	TArray<FHanjiBox> GetActiveBoxes() const;

	/** World position of a pixel of the current cell (a hand, a spark point), on the quad. */
	UFUNCTION(BlueprintPure, Category = "Hanji")
	FVector GetPixelWorldLocation(FVector2D Pixel) const;

	/**
	 * Keeps a component (an effect, a weapon trail, a light) on a pixel of the cell, given as drawn in the
	 * unmirrored art: when a heading is drawn mirrored the pixel follows the mirror, and when the heading
	 * changes the component follows the sprite. DepthBias moves it toward the camera, in front of the sprite.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void AttachToPixel(USceneComponent* Component, FVector2D Pixel, float DepthBias = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void DetachFromPixel(USceneComponent* Component);

	UDecalComponent* GetBlobShadow() const { return Shadow; }

	/** Advances the clock and updates the quad. Called by UHanjiSubsystem; call it yourself only in tests. */
	void HanjiAdvance(float DeltaSeconds, const FRotator& ViewRotation);

	/** The UV rectangle (u, v, width, height) and mirror flag last sent to the material. */
	FVector4 GetFrameUV() const { return LastUV; }
	bool IsMirrored() const { return bLastMirror; }

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	bool EnsurePlayer();
	bool PlayWith(FName Action, const struct FHanjiPlayRequest& Request);
	void UpdateVisual(const FRotator& ViewRotation, bool bForce);
	void BroadcastEvents();
	float PixelSize() const;
	FVector AnchorLocation() const;

	// Shared, not unique: the type-erased deleter lets generated code destroy it without the full type.
	TSharedPtr<hanji::Player> Player;
	const hanji::StatePolicy* StatePolicy = nullptr;
	FName CurrentState;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Shadow;

	struct FPixelAttachment
	{
		TWeakObjectPtr<USceneComponent> Component;
		FVector2D Pixel;
		float DepthBias = 0.0f;
	};
	TArray<FPixelAttachment> Attachments;
	void UpdateShadowAndAttachments(const FVector& Anchor, const FRotator& ViewRotation);

	FRotator LastView = FRotator(-30.0, 45.0, 0.0);
	FVector LastFacing = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;
	FVector4 LastUV = FVector4(0, 0, 0, 0);
	bool bLastMirror = false;
	int32 LastFrame = INDEX_NONE;
	int32 LastHeading = INDEX_NONE;
	uint64 FinishedPlayId = 0;
};
