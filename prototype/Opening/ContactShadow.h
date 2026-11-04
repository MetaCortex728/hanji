#pragma once

#include "CoreMinimal.h"
#include "PaperSpriteComponent.h"
#include "ContactShadow.generated.h"

class AGyeolQuarterViewCamera;

namespace GyeolContact
{
    inline uint8 Opacity(int32 X, int32 Y, int32 Size)
    {
        if (Size < 2 || X < 0 || Y < 0 || X >= Size || Y >= Size) { return 0; }
        const double U = (2.0 * X + 1.0 - Size) / (Size - 2.0 + 0.001);
        const double V = (2.0 * Y + 1.0 - Size) / (Size - 2.0 + 0.001);
        const double Weight = FMath::Max(0.0, 1.0 - U * U - V * V);
        return uint8(FMath::RoundToInt(88.0 * Weight * Weight));
    }
}

// Flat-floor contact cue for the painted opening, not a light-cast terrain shadow.
UCLASS()
class GYEOLLAB_API UGyeolContactShadowComponent : public UPaperSpriteComponent
{
    GENERATED_BODY()
public:
    UGyeolContactShadowComponent();
    bool Initialize(AGyeolQuarterViewCamera* View, double FootOffset, double Diameter);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    FVector FootPosition() const;
    double GetDiameter() const { return DiameterCm; }
private:
    UPROPERTY(Transient) TObjectPtr<AGyeolQuarterViewCamera> Camera;
    double OffsetCm = 85.0;
    double DiameterCm = 90.0;
};
