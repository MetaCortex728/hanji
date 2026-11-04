// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HanjiClipTable.generated.h"

class UTexture2D;
namespace hanji { struct ClipTable; struct PolicyTable; }

UENUM(BlueprintType)
enum class EHanjiSourceFormat : uint8
{
	/** Detected from the JSON: a pose-atlas contract, Aseprite JSON or hanji-clips. */
	Auto,
	PoseContract,
	AsepriteJson,
	HanjiClips
};

/**
 * One character's atlas: the image, the metadata describing its actions in eight headings, and optional
 * overlays for contact frames, events, hitboxes and per-state policies. The JSON is stored in the asset,
 * so packaged games need no loose files; in the editor, Reload From Files reads it again from disk.
 */
UCLASS(BlueprintType)
class HANJI_API UHanjiClipTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UHanjiClipTable();
	virtual ~UHanjiClipTable() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hanji")
	TObjectPtr<UTexture2D> Atlas;

	UPROPERTY(EditAnywhere, Category = "Hanji")
	EHanjiSourceFormat Format = EHanjiSourceFormat::Auto;

	/** pose-contract.json, an Aseprite JSON export, or hanji-clips JSON. */
	UPROPERTY(EditAnywhere, Category = "Hanji", meta = (MultiLine = true))
	FString SourceJson;

	/** Optional hanji-clips overlay: contact frames, events, hitboxes. */
	UPROPERTY(EditAnywhere, Category = "Hanji", meta = (MultiLine = true))
	FString OverlayJson;

	/** Optional hanji-policies: what each gameplay state means for facing, stride, timing and hit-stop. */
	UPROPERTY(EditAnywhere, Category = "Hanji", meta = (MultiLine = true))
	FString PoliciesJson;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Hanji|Source Files", meta = (FilePathFilter = "json"))
	FFilePath SourceFile;

	UPROPERTY(EditAnywhere, Category = "Hanji|Source Files", meta = (FilePathFilter = "json"))
	FFilePath OverlayFile;

	UPROPERTY(EditAnywhere, Category = "Hanji|Source Files", meta = (FilePathFilter = "json"))
	FFilePath PoliciesFile;
#endif

	/** What the last build of the table reported. Errors leave the table unusable. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Hanji")
	TArray<FString> Errors;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Hanji")
	TArray<FString> Warnings;

#if WITH_EDITOR
	/** Reads the source, overlay and policy files into the stored JSON and rebuilds the table. */
	UFUNCTION(CallInEditor, Category = "Hanji")
	void ReloadFromFiles();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	virtual void PostLoad() override;

	/** Parses the stored JSON. Returns true when the table is usable. */
	UFUNCTION(BlueprintCallable, Category = "Hanji")
	bool Rebuild();

	UFUNCTION(BlueprintPure, Category = "Hanji")
	TArray<FName> GetActionNames() const;

	UFUNCTION(BlueprintPure, Category = "Hanji")
	bool IsUsable() const { return Table.IsValid(); }

	/** The parsed table, or null when the JSON did not build. */
	const hanji::ClipTable* GetTable() const { return Table.Get(); }
	/** The parsed policies, or null when there are none. */
	const hanji::PolicyTable* GetPolicies() const { return Policies.Get(); }

	/** Centimetres per atlas pixel from the metadata (pose-atlas states it), or 0 when unknown. */
	float GetCentimetresPerPixel() const;

private:
	TSharedPtr<hanji::ClipTable> Table;
	TSharedPtr<hanji::PolicyTable> Policies;
};
