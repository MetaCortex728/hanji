// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HanjiSubsystem.generated.h"

class UHanjiSpriteComponent;

/**
 * Advances every Hanji sprite in the world once per frame, so the sprites themselves never tick: one
 * loop over plain data instead of one tick per actor. The view rotation comes from the first local
 * player's camera, or from SetViewRotation (editor preview, tests, cut scenes).
 */
UCLASS()
class HANJI_API UHanjiSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(UHanjiSpriteComponent* Sprite);
	void Unregister(UHanjiSpriteComponent* Sprite);

	/** Uses this view rotation instead of the player camera's. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void SetViewRotation(FRotator Rotation);

	/** Returns to following the player camera. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	void ClearViewRotation();

	UFUNCTION(BlueprintPure, Category = "Hanji")
	FRotator GetViewRotation() const;

	int32 NumSprites() const { return Sprites.Num(); }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }

private:
	TArray<TWeakObjectPtr<UHanjiSpriteComponent>> Sprites;
	TOptional<FRotator> ViewOverride;
	FRotator DefaultView = FRotator(-30.0, 45.0, 0.0);
};
