// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HanjiAssetLibrary.generated.h"

class UStaticMesh;
struct FMeshDescription;

UCLASS()
class HANJI_API UHanjiAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Default assets shipped with the plugin. */
	static constexpr const TCHAR* QuadMeshPath = TEXT("/Hanji/Meshes/SM_HanjiQuad.SM_HanjiQuad");
	static constexpr const TCHAR* LitMaterialPath = TEXT("/Hanji/Materials/M_HanjiSprite.M_HanjiSprite");
	static constexpr const TCHAR* UnlitMaterialPath = TEXT("/Hanji/Materials/M_HanjiSpriteUnlit.M_HanjiSpriteUnlit");
	static constexpr const TCHAR* BlobMaterialPath = TEXT("/Hanji/Materials/M_HanjiBlobShadow.M_HanjiBlobShadow");

	/**
	 * The sprite quad: one unit wide (local Y from -0.5 to 0.5) and one unit high (local Z from 0 to 1),
	 * flat in local X with its normal along -X, the ground pivot at the origin, UV (0, 0) at the top left.
	 */
	static void FillQuadMeshDescription(FMeshDescription& Description);

	/** Editor only: creates the quad mesh asset at a package path such as /Hanji/Meshes/SM_HanjiQuad. */
	UFUNCTION(BlueprintCallable, Category = "Hanji|Editor")
	static UStaticMesh* CreateQuadMeshAsset(const FString& PackagePath);
};
