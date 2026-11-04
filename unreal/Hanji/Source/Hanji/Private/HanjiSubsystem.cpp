// SPDX-License-Identifier: Apache-2.0
#include "HanjiSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HanjiSpriteComponent.h"

void UHanjiSubsystem::Register(UHanjiSpriteComponent* Sprite)
{
	Sprites.AddUnique(Sprite);
}

void UHanjiSubsystem::Unregister(UHanjiSpriteComponent* Sprite)
{
	Sprites.Remove(Sprite);
}

void UHanjiSubsystem::SetViewRotation(FRotator Rotation)
{
	ViewOverride = Rotation;
}

void UHanjiSubsystem::ClearViewRotation()
{
	ViewOverride.Reset();
}

FRotator UHanjiSubsystem::GetViewRotation() const
{
	if (ViewOverride) return *ViewOverride;
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager) return PC->PlayerCameraManager->GetCameraRotation();
		}
	}
	return DefaultView;
}

void UHanjiSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const FRotator View = GetViewRotation();
	for (int32 I = Sprites.Num() - 1; I >= 0; --I)
	{
		if (UHanjiSpriteComponent* Sprite = Sprites[I].Get())
		{
			Sprite->HanjiAdvance(DeltaTime, View);
		}
		else
		{
			Sprites.RemoveAtSwap(I);
		}
	}
}

TStatId UHanjiSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHanjiSubsystem, STATGROUP_Tickables);
}
