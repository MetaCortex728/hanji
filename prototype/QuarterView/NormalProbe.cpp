#include "PixelProbe.h"
#include "QuarterViewCamera.h"

#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "ShaderCompilerCore.h"

namespace
{
template<class T> T* Node(UMaterial* Material)
{
    T* Result = NewObject<T>(Material);
    Material->GetExpressionCollection().AddExpression(Result);
    return Result;
}
}
#endif

bool UGyeolPixelProbeComponent::InitializeNormalMap(const FString& Path, bool bFlatControl, bool bDebugVectors)
{
#if WITH_EDITOR
    const auto* Sprite = GetLabSprite();
    FImage Image;
    if (!IsValid(Camera) || !Sprite || !FImageUtils::LoadImage(*Path, Image)
        || Image.NumSlices != 1 || Image.SizeX != Sprite->GetBakedTexture()->GetSizeX()
        || Image.SizeY != Sprite->GetBakedTexture()->GetSizeY()) { return false; }
    // Reinterpret the PNG as linear vector data. A gamma conversion corrupts normals.
    Image.GammaSpace = EGammaSpace::Linear;
    Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::Linear);
    UTexture2D* Texture = UTexture2D::CreateTransientFromImage(&Image);
    if (!Texture) { return false; }
    Texture->SRGB = false;
    Texture->Filter = TF_Nearest;
    Texture->AddressX = Texture->AddressY = TA_Clamp;
    Texture->NeverStream = true;
    Texture->UpdateResource();

    auto* Material = NewObject<UMaterial>(this);
    Material->BlendMode = BLEND_Masked;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = true;
    Material->bTangentSpaceNormal = false;
    auto* Color = Node<UMaterialExpressionTextureSample>(Material);
    Color->Texture = Sprite->GetBakedTexture();
    Color->SamplerType = SAMPLERTYPE_Color;
    auto* Normal = Node<UMaterialExpressionTextureSample>(Material);
    Normal->Texture = Texture;
    Normal->SamplerType = SAMPLERTYPE_LinearColor;

    // RGB stores screen-right/up/toward-camera, not a DirectX tangent normal.
    const FRotationMatrix Basis(Camera->ViewRotation);
    const FVector Axes[] = {Basis.GetUnitAxis(EAxis::Y), Basis.GetUnitAxis(EAxis::Z), -Basis.GetUnitAxis(EAxis::X)};
    UMaterialExpression* Terms[3];
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        auto* Vector = Node<UMaterialExpressionConstant3Vector>(Material);
        Vector->Constant = FLinearColor(Axes[Axis] * 2.0);
        auto* Product = Node<UMaterialExpressionMultiply>(Material);
        Product->A.Connect(Axis + 1, Normal);
        Product->B.Connect(0, Vector);
        Terms[Axis] = Product;
    }
    auto* XY = Node<UMaterialExpressionAdd>(Material);
    XY->A.Connect(0, Terms[0]); XY->B.Connect(0, Terms[1]);
    auto* XYZ = Node<UMaterialExpressionAdd>(Material);
    XYZ->A.Connect(0, XY); XYZ->B.Connect(0, Terms[2]);
    auto* Offset = Node<UMaterialExpressionConstant3Vector>(Material);
    Offset->Constant = FLinearColor(-Axes[0] - Axes[1] - Axes[2]);
    auto* Decoded = Node<UMaterialExpressionAdd>(Material);
    Decoded->A.Connect(0, XYZ); Decoded->B.Connect(0, Offset);
    auto* Zero = Node<UMaterialExpressionConstant>(Material); Zero->R = 0.0f;
    auto* One = Node<UMaterialExpressionConstant>(Material); One->R = 1.0f;
    auto* Data = Material->GetEditorOnlyData();
    Data->BaseColor.Connect(0, Color);
    Data->OpacityMask.Connect(4, Color);
    Data->Normal.Connect(0, Decoded);
    if (bFlatControl)
    {
        auto* Flat = Node<UMaterialExpressionConstant3Vector>(Material);
        Flat->Constant = FLinearColor(Axes[2]);
        Data->Normal.Connect(0, Flat);
    }
    Data->Specular.Connect(0, Zero);
    Data->Roughness.Connect(0, One);
    if (bDebugVectors)
    {
        Material->SetShadingModel(MSM_Unlit);
        Data->EmissiveColor.Connect(0, Normal);
    }
    Material->PostEditChange();
    auto* Resource = Material->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel()));
    if (!Resource) { return false; }
    Resource->FinishCacheShaders();
    Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
    Resource->FinishCompilation();
    if (Resource->GetCompileErrors().Num()) { return false; }
    NormalMaterial = Material;
    return SetLit(true);
#else
    return false;
#endif
}
