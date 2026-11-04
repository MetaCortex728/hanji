#include "../Opening/ContactShadow.h"
#include "../QuarterView/QuarterViewMath.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContactShadowTest, "Gyeol.Art.ContactShadow",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FContactShadowTest::RunTest(const FString& Parameters)
{
    constexpr int32 Size = 32;
    for (int32 Y = 0; Y < Size; ++Y)
    {
        for (int32 X = 0; X < Size; ++X)
        {
            const uint8 A = GyeolContact::Opacity(X, Y, Size);
            TestTrue(TEXT("Limited opacity"), A <= 88);
            TestEqual(TEXT("Horizontal symmetry"), A, GyeolContact::Opacity(Size - 1 - X, Y, Size));
            TestEqual(TEXT("Vertical symmetry"), A, GyeolContact::Opacity(X, Size - 1 - Y, Size));
            if (X == 0 || Y == 0 || X == Size - 1 || Y == Size - 1) { TestEqual(TEXT("Clear border"), int32(A), 0); }
        }
    }
    TestTrue(TEXT("Visible center"), GyeolContact::Opacity(16, 16, Size) > 80);
    for (int32 X = 16; X < Size - 1; ++X)
    { TestTrue(TEXT("Soft falloff"), GyeolContact::Opacity(X, 16, Size) >= GyeolContact::Opacity(X + 1, 16, Size)); }
    for (const double Zoom : {.9, 1.0, 1.1})
    {
        const FRotator View(-30, 45, 0);
        const FVector Foot(123.4, -87.9, 0), Camera(-900, -800, 700);
        const FVector Anchor = GyeolQuarterView::SnapGroundAnchor(Foot, Camera, View, 2080.0 * Zoom / 960);
        const FVector Lifted = Anchor - View.Vector() * 1.5;
        TestTrue(TEXT("Lift does not shift screen foot"), FVector::CrossProduct(Lifted - Anchor, View.Vector()).IsNearlyZero());
        TestTrue(TEXT("Above flat floor"), Lifted.Z > 0 && Lifted.Z < 1);
    }
    return true;
}
#endif
