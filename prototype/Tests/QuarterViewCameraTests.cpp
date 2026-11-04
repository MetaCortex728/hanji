#include "../QuarterView/QuarterViewMath.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuarterViewAppearanceResolutionTest, "Gyeol.Camera.AppearanceResolution",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQuarterViewAppearanceResolutionTest::RunTest(const FString& Parameters)
{
    using namespace GyeolQuarterView;
    TestTrue(TEXT("Existing low density remains supported"), IsStudyResolutionSupported(640, false));
    TestTrue(TEXT("Existing Dan density remains supported"), IsStudyResolutionSupported(960, false));
    TestFalse(TEXT("Native resolution is not silently adopted"), IsStudyResolutionSupported(1920, false));
    TestTrue(TEXT("Native fixture accepts its explicit density"), IsStudyResolutionSupported(1920, true));
    TestFalse(TEXT("Native fixture rejects low density"), IsStudyResolutionSupported(960, true));
    TestFalse(TEXT("Unknown density fails closed"), IsStudyResolutionSupported(1234, true));
    for (double View : {0.9, 1.0, 1.1})
    {
        const double Width = 2080 * View;
        const double LowHeight = 72 / View, NativeHeight = 144 / View;
        TestTrue(TEXT("Both modes preserve a 156 cm projected character"),
            FMath::IsNearlyEqual(LowHeight * Width / 960, 156.0, 1.e-8) &&
            FMath::IsNearlyEqual(NativeHeight * Width / 1920, 156.0, 1.e-8));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuarterViewZoomTest, "Gyeol.Camera.ZoomScale",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQuarterViewZoomTest::RunTest(const FString& Parameters)
{
    using namespace GyeolQuarterView;
    FZoom Zoom;
    TestEqual(TEXT("Default view is unchanged"), Zoom.Width(2080), 2080.0);
    Zoom.Set(2);
    TestEqual(TEXT("Zoom is capped at 110 percent"), Zoom.Target, 1.1);
    Zoom.Tick(0);
    TestEqual(TEXT("Paused time does not advance zoom"), Zoom.Current, 1.0);
    Zoom.Tick(.06);
    TestTrue(TEXT("Transition halves the gap without jumping"), FMath::IsNearlyEqual(Zoom.Current, 1.05, 1.e-8));
    for (int32 Rate : {30, 60, 120})
    {
        FZoom Candidate; Candidate.Set(1.1);
        for (int32 I=0; I<Rate; ++I) { Candidate.Tick(1.0/Rate); }
        TestEqual(TEXT("Transition settles exactly at any tested rate"), Candidate.Current, 1.1);
        TestTrue(TEXT("110 percent widens the view to 2288 cm"), FMath::IsNearlyEqual(Candidate.Width(2080), 2288.0, 1.e-8));
        const double Density=Candidate.Width(2080)/640;
        TestTrue(TEXT("Same world-space height projects smaller in the wider view"), FMath::IsNearlyEqual((156/Density)/(156/3.25), 1.0/1.1, 1.e-8));
        const FRotator View(-30,45,0);
        const FVector Point(39.17,-21.23,0), Origin(8.3,4.5,7.1);
        const FVector Snapped=SnapGroundAnchor(Point,Origin,View,Density);
        TestEqual(TEXT("Zoomed visual snap preserves ground height"), Snapped.Z, Point.Z);
        const FRotationMatrix Basis(View);
        for (auto Axis : {EAxis::Y,EAxis::Z})
        {
            const double Pixel=FVector::DotProduct(Snapped-Origin,Basis.GetUnitAxis(Axis))/Density;
            TestTrue(TEXT("Zoomed anchors use the effective pixel lattice"), FMath::IsNearlyEqual(Pixel,FMath::RoundToDouble(Pixel),1.e-6));
        }
        const FVector2D Base(520,270);
        const auto Bounds=ZoomFocusBounds(Base,2080,Candidate.Width(2080));
        TestTrue(TEXT("Zoom-out retains original horizontal and vertical backdrop coverage"),
            FMath::IsNearlyEqual(Bounds.X+Candidate.Width(2080)*.5,Base.X+1040,1.e-6)
            && FMath::IsNearlyEqual(Bounds.Y+Candidate.Width(2080)*9/32,Base.Y+2080.0*9/32,1.e-6));
        Candidate.Set(.8); Candidate.Tick(2);
        TestEqual(TEXT("Close view stops at 90 percent"), Candidate.Current, .9);
        TestTrue(TEXT("90 percent narrows the view to 1872 cm"), FMath::IsNearlyEqual(Candidate.Width(2080),1872.0,1.e-8));
        TestTrue(TEXT("Same world-space height projects larger in the closer view"),
            FMath::IsNearlyEqual(2080/Candidate.Width(2080),1.0/.9,1.e-8));
        TestTrue(TEXT("Close view does not extend camera roaming bounds"), ZoomFocusBounds(Base,2080,Candidate.Width(2080)).Equals(Base));
        TestTrue(TEXT("Very small room margins collapse instead of becoming negative"), ZoomFocusBounds(FVector2D(2,2),2080,2288).IsNearlyZero());
        Candidate.Set(1); Candidate.Tick(2);
        TestEqual(TEXT("Reset restores original width"), Candidate.Width(2080), 2080.0);
    }
    Zoom.Set(std::numeric_limits<double>::quiet_NaN());
    TestEqual(TEXT("Invalid preference does not poison state"), Zoom.Target, 1.1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuarterViewFollowTest, "Gyeol.Camera.FollowTiming",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQuarterViewFollowTest::RunTest(const FString& Parameters)
{
    const FVector Start(-40.0, 20.0, 80.0);
    const FVector Target(160.0, 220.0, 180.0);
    TestTrue(TEXT("One half-life halves the remaining error"),
        GyeolQuarterView::Follow(Start, Target, 0.08, 0.08).Equals((Start + Target) * 0.5, 0.0001));
    TestTrue(TEXT("A zero delta does not advance"),
        GyeolQuarterView::Follow(Start, Target, 0.0, 0.08).Equals(Start));
    TestTrue(TEXT("A negative delta does not advance"),
        GyeolQuarterView::Follow(Start, Target, -0.1, 0.08).Equals(Start));
    TestTrue(TEXT("Zero smoothing snaps to the target"),
        GyeolQuarterView::Follow(Start, Target, 0.01, 0.0).Equals(Target));

    const FVector Expected = GyeolQuarterView::Follow(Start, Target, 1.0, 0.08);
    for (const int32 Rate : {30, 60, 120})
    {
        FVector Position = Start;
        for (int32 Step = 0; Step < Rate; ++Step)
        {
            Position = GyeolQuarterView::Follow(Position, Target, 1.0 / Rate, 0.08);
        }
        TestTrue(FString::Printf(TEXT("Static-target response matches at %dfps"), Rate),
            Position.Equals(Expected, 0.0001));
    }
    const FVector Spike = GyeolQuarterView::Follow(Start, Target, 0.5, 0.08);
    TestTrue(TEXT("Frame spikes do not overshoot"),
        FVector::Dist(Spike, Target) < FVector::Dist(Start, Target)
        && FVector::DotProduct(Spike - Start, Target - Spike) >= 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuarterViewInputTest, "Gyeol.Camera.ScreenRelativeInput",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQuarterViewInputTest::RunTest(const FString& Parameters)
{
    const FVector Up = GyeolQuarterView::GroundDirection(FVector2D(0.0, 1.0), 45.0);
    const FVector Right = GyeolQuarterView::GroundDirection(FVector2D(1.0, 0.0), 45.0);
    TestTrue(TEXT("Screen up follows the horizontal view-forward axis"),
        Up.Equals(FVector(1.0, 1.0, 0.0).GetSafeNormal(), 0.0001));
    TestTrue(TEXT("Screen right follows the view-right axis"),
        Right.Equals(FVector(-1.0, 1.0, 0.0).GetSafeNormal(), 0.0001));
    TestTrue(TEXT("Diagonal input is not faster"),
        FMath::IsNearlyEqual(GyeolQuarterView::GroundDirection(FVector2D(1.0, 1.0), 45.0).Size(), 1.0, 0.0001));
    TestTrue(TEXT("Analog magnitude is preserved"),
        FMath::IsNearlyEqual(GyeolQuarterView::GroundDirection(FVector2D(0.3, 0.4), 45.0).Size(), 0.5, 0.0001));
    TestTrue(TEXT("No input produces no direction"),
        GyeolQuarterView::GroundDirection(FVector2D::ZeroVector, 45.0).IsNearlyZero());
    for (const double Yaw : {0.0, 45.0, 90.0, -135.0})
    {
        TestTrue(TEXT("Camera yaw never adds vertical movement"),
            FMath::IsNearlyZero(GyeolQuarterView::GroundDirection(FVector2D(0.6, 0.8), Yaw).Z));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuarterViewSnapTest, "Gyeol.Camera.RenderPlaneSnap",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQuarterViewSnapTest::RunTest(const FString& Parameters)
{
    const FVector Origin(17.25, -8.5, 23.75);
    for (double Pitch : {-30.0, -35.0})
    {
        const FRotator Rotation(Pitch, 45.0, 0.0);
        const FRotationMatrix Basis(Rotation);
        const FVector Forward = Basis.GetUnitAxis(EAxis::X);
        const FVector Right = Basis.GetUnitAxis(EAxis::Y);
        const FVector Up = Basis.GetUnitAxis(EAxis::Z);
        constexpr double Q = 3.25;
        for (int32 Step = -100; Step <= 100; ++Step)
        {
            const FVector Point = Origin + Right * (Step * 0.17) + Up * (Step * 0.29) + Forward * 47.25;
            const FVector Snapped = GyeolQuarterView::SnapViewPlane(Point, Origin, Rotation, Q);
            for (const FVector Axis : {Right, Up})
            {
                const double Pixel = FVector::DotProduct(Snapped - Origin, Axis) / Q;
                TestTrue(TEXT("Visual point lands on the view-plane pixel lattice"),
                    FMath::IsNearlyEqual(Pixel, FMath::RoundToDouble(Pixel), 0.00001));
                TestTrue(TEXT("Per-axis render offset is at most half a pixel"),
                    FMath::Abs(FVector::DotProduct(Snapped - Point, Axis)) <= Q * 0.5 + 0.00001);
            }
            TestTrue(TEXT("Snap does not change camera depth"),
                FMath::IsNearlyZero(FVector::DotProduct(Snapped - Point, Forward), 0.00001));
            TestTrue(TEXT("Snap is idempotent"),
                GyeolQuarterView::SnapViewPlane(Snapped, Origin, Rotation, Q).Equals(Snapped, 0.00001));
            const FVector GroundSnapped = GyeolQuarterView::SnapGroundAnchor(Point, Origin, Rotation, Q);
            TestTrue(TEXT("Ground anchor keeps the original floor height"),
                FMath::IsNearlyEqual(GroundSnapped.Z, Point.Z, 0.00001));
            TestTrue(TEXT("Ground correction preserves the snapped screen coordinates"),
                FMath::IsNearlyZero(FVector::DotProduct(GroundSnapped - Snapped, Right), 0.00001)
                && FMath::IsNearlyZero(FVector::DotProduct(GroundSnapped - Snapped, Up), 0.00001));
        }
    }
    TestTrue(TEXT("Invalid density leaves the point unchanged"),
        GyeolQuarterView::SnapViewPlane(Origin, FVector::ZeroVector, FRotator::ZeroRotator, 0.0).Equals(Origin));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuarterViewSpriteBasisTest, "Gyeol.Camera.SpriteFacing",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FQuarterViewSpriteBasisTest::RunTest(const FString& Parameters)
{
    for (double Pitch : {-30.0, -35.0})
    {
        const FRotator View(Pitch, 45.0, 0.0);
        const FRotationMatrix Basis(View);
        const FQuat Facing = GyeolQuarterView::SpriteFacingRotation(View);
        TestTrue(TEXT("Paper2D local -Y normal points toward the camera"),
            Facing.RotateVector(FVector(0.0, -1.0, 0.0)).Equals(-Basis.GetUnitAxis(EAxis::X), 0.00001));
        TestTrue(TEXT("Image up is screen up"),
            Facing.RotateVector(FVector::UpVector).Equals(Basis.GetUnitAxis(EAxis::Z), 0.00001));
        TestTrue(TEXT("Reversed quad X preserves image left and right"),
            Facing.RotateVector(FVector(-1.0, 0.0, 0.0)).Equals(Basis.GetUnitAxis(EAxis::Y), 0.00001));
    }
    return true;
}
#endif
