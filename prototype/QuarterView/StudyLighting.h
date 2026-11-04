#pragma once

#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "TextureResource.h"

namespace GyeolStudyLighting
{
    // Neutral bounce approximation; no albedo edits, occlusion or real environment capture.
    inline bool AddNeutralFill(UWorld* World, float Intensity)
    {
        if (!World || !FMath::IsFinite(Intensity) || Intensity < 0.0f || Intensity > 2.0f) { return false; }
        if (Intensity == 0.0f) { return true; }
        constexpr int32 Size = 16;
        UTextureCube* Environment = UTextureCube::CreateTransient(Size, Size, PF_B8G8R8A8);
        if (!Environment) { return false; }
        Environment->SRGB = false;
        Environment->NeverStream = true;
        auto& Mip = Environment->GetPlatformData()->Mips[0];
        auto* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
        for (int32 Index = 0; Index < 6 * Size * Size; ++Index) { Pixels[Index] = FColor::White; }
        Mip.BulkData.Unlock();
        Environment->UpdateResource();
        ASkyLight* Sky = World->SpawnActor<ASkyLight>();
        if (!Sky) { return false; }
        USkyLightComponent* Light = Sky->GetLightComponent();
        Light->SetMobility(EComponentMobility::Movable);
        Light->SourceType = SLS_SpecifiedCubemap;
        Light->CubemapResolution = 32;
        Light->bLowerHemisphereIsBlack = false;
        Light->SetCastShadows(false);
        Light->SetIntensity(Intensity);
        Light->SetCubemap(Environment);
        return true;
    }
}
