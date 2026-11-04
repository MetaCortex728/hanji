// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HanjiTestListener.generated.h"

/** Records a sprite's events for the automation tests. */
UCLASS(Transient)
class UHanjiTestListener : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void OnEvent(FName Event, FName Action, int32 Frame) { Events.Add(FString::Printf(TEXT("%s@%d"), *Event.ToString(), Frame)); }

	UFUNCTION()
	void OnFinished(FName Action) { Finished.Add(Action); }

	TArray<FString> Events;
	TArray<FName> Finished;
};
