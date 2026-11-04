// SPDX-License-Identifier: Apache-2.0
#include "HanjiAssetLibrary.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

void UHanjiAssetLibrary::FillQuadMeshDescription(FMeshDescription& Description)
{
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> Signs = Attributes.GetVertexInstanceBinormalSigns();
	UVs.SetNumChannels(1);

	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Sprite");

	// Corners: bottom left, bottom right, top right, top left (seen from the camera, which looks along +X).
	const FVector3f Corners[4] = {{0, -0.5f, 0}, {0, 0.5f, 0}, {0, 0.5f, 1}, {0, -0.5f, 1}};
	const FVector2f Texture[4] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
	FVertexInstanceID Instances[4];
	for (int32 I = 0; I < 4; ++I)
	{
		const FVertexID Vertex = Description.CreateVertex();
		Positions[Vertex] = Corners[I];
		Instances[I] = Description.CreateVertexInstance(Vertex);
		UVs.Set(Instances[I], 0, Texture[I]);
		Normals[Instances[I]] = FVector3f(-1, 0, 0);
		Tangents[Instances[I]] = FVector3f(0, 1, 0);
		Signs[Instances[I]] = 1.0f;
	}
	Description.CreateTriangle(Group, {Instances[0], Instances[2], Instances[1]});
	Description.CreateTriangle(Group, {Instances[0], Instances[3], Instances[2]});
}

UStaticMesh* UHanjiAssetLibrary::CreateQuadMeshAsset(const FString& PackagePath)
{
#if WITH_EDITOR
	UPackage* Package = CreatePackage(*PackagePath);
	const FName Name(*FPackageName::GetShortName(PackagePath));
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, Name, RF_Public | RF_Standalone);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Sprite"), TEXT("Sprite")));

	FMeshDescription Description;
	FillQuadMeshDescription(Description);
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bCommitMeshDescription = true;
	Params.bMarkPackageDirty = true;
	if (!Mesh->BuildFromMeshDescriptions({&Description}, Params))
	{
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Mesh);
	return Mesh;
#else
	return nullptr;
#endif
}
