#include "PixelProbe.h"
#include "QuarterViewCamera.h"

#if WITH_EDITOR
#include "Camera/CameraComponent.h"
#include "HAL/IConsoleManager.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionSceneTexture.h"
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

bool UGyeolPixelProbeComponent::InitializeOcclusionHint(bool bDiagnosticMask)
{
#if WITH_EDITOR
    if (!IsValid(Camera) || !GetSprite() || OcclusionMaterial) { return false; }
    auto* CustomDepth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
    if (!CustomDepth) { return false; }
    CustomDepth->Set(3, ECVF_SetByCode);

    auto* Material = NewObject<UMaterial>(this);
    Material->MaterialDomain = MD_PostProcess;
    Material->BlendableLocation = BL_SceneColorAfterTonemapping;
    auto* Composite = Node<UMaterialExpressionCustom>(Material);
    Composite->OutputType = CMOT_Float3;
    Composite->Description = TEXT("Player-only occluded silhouette; no temporal filtering");
    Composite->Inputs.Reset();
    // SceneTexture inputs declare dependencies; neighbor reads use the same custom-stencil buffer.
    const ESceneTextureId TextureIds[] = {PPI_PostProcessInput0, PPI_SceneDepth, PPI_CustomDepth, PPI_CustomStencil};
    const TCHAR* InputNames[] = {TEXT("Scene"), TEXT("Depth"), TEXT("PlayerDepth"), TEXT("PlayerStencil")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Texture = Node<UMaterialExpressionSceneTexture>(Material);
        Texture->SceneTextureId = TextureIds[Index];
        FCustomInput Input;
        Input.InputName = InputNames[Index];
        Input.Input.Connect(0, Texture);
        Composite->Inputs.Add(Input);
    }
    auto* Debug = Node<UMaterialExpressionConstant>(Material);
    Debug->R = bDiagnosticMask ? 1.0f : 0.0f;
    FCustomInput DebugInput;
    DebugInput.InputName = TEXT("DebugMask");
    DebugInput.Input.Connect(0, Debug);
    Composite->Inputs.Add(DebugInput);
    Composite->Code = TEXT(R"(
float player = abs(PlayerStencil.r - 79.0) < 0.5;
float hidden = player * (PlayerDepth.r > Depth.r + 2.0);
if (DebugMask > 0.5)
    return float3(hidden, player * (1.0 - hidden), 0.0);
if (hidden < 0.5)
    return Scene.rgb;
float2 uv = GetDefaultSceneTextureUV(Parameters, PPI_CustomStencil);
float2 pixel = GetSceneTextureBufferSize(PPI_CustomStencil).zw;
float inside = 1.0;
inside *= abs(SceneTextureLookup(uv + float2(pixel.x, 0), PPI_CustomStencil, false).r - 79.0) < 0.5;
inside *= abs(SceneTextureLookup(uv - float2(pixel.x, 0), PPI_CustomStencil, false).r - 79.0) < 0.5;
inside *= abs(SceneTextureLookup(uv + float2(0, pixel.y), PPI_CustomStencil, false).r - 79.0) < 0.5;
inside *= abs(SceneTextureLookup(uv - float2(0, pixel.y), PPI_CustomStencil, false).r - 79.0) < 0.5;
float luminance = dot(Scene.rgb, float3(0.2126, 0.7152, 0.0722));
float3 rim = luminance > 0.35 ? float3(0.025, 0.06, 0.05) : float3(0.70, 0.82, 0.76);
return lerp(Scene.rgb, lerp(rim, float3(0.70, 0.82, 0.76), inside), lerp(0.90, 0.22, inside));
)");
    Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Composite);
    Material->PostEditChange();
    auto* Resource = Material->GetMaterialResource(GetFeatureLevelShaderPlatform_Checked(GetWorld()->GetFeatureLevel()));
    if (!Resource) { return false; }
    Resource->FinishCacheShaders();
    Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
    Resource->FinishCompilation();
    if (Resource->GetCompileErrors().Num()) { return false; }
    OcclusionMaterial = Material;
    SetCustomDepthStencilValue(79);
    SetRenderCustomDepth(true);
    Camera->GetCameraComponent()->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Material));
    return true;
#else
    return false;
#endif
}
