#include "../QuarterView/PoseAtlas.h"
#include "../QuarterView/StrideClock.h"
#include "../QuarterView/LocomotionStop.h"
#include "../Combat/StrikeCaptureSchedule.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanNativeTimingTest, "Gyeol.Art.DanNativeTiming",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanNativeTimingTest::RunTest(const FString& Parameters)
{
    const GyeolPose::FNativeStrike Strike{104, 31, 49};
    for (int32 Rate : {30, 60, 120, 240})
    {
        int32 Previous = 0;
        for (int32 Tick = 0; Tick <= Rate * 2; ++Tick)
        {
            const double Time = double(Tick) / Rate;
            TestEqual(TEXT("Source timing preserves every original sample"),
                Strike.TimedFrame(Time, 31./60, 18./60, 55./60), Strike.Frame(Time));
            const int32 Sample = Strike.TimedFrame(Time, 9./60, 6./60, 18./60);
            TestTrue(TEXT("Combat playback stays bounded and ordered across frame rates"),
                Sample >= Previous && Sample < 104);
            Previous = Sample;
        }
    }
    TestEqual(TEXT("Exact contact aligns with damage opening"), Strike.TimedFrame(.15, .15, .1, .3), 31);
    TestEqual(TEXT("Exact active end aligns with recovery opening"), Strike.TimedFrame(.25, .15, .1, .3), 49);
    TestEqual(TEXT("Full playback end clamps to last source drawing"), Strike.TimedFrame(.55, .15, .1, .3), 103);
    TestEqual(TEXT("Contact pose name remains stable for hit-stop"), Strike.TimedPose(.15, .15, .1, .3), FName(TEXT("jab31")));
    for (double Bad : {-1., 0., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        TestEqual(TEXT("Invalid startup rejected"), Strike.TimedFrame(.1, Bad, .1, .3), 0);
        TestEqual(TEXT("Invalid active rejected"), Strike.TimedFrame(.1, .15, Bad, .3), 0);
        TestEqual(TEXT("Invalid recovery rejected"), Strike.TimedFrame(.1, .15, .1, Bad), 0);
    }
    TestEqual(TEXT("Invalid time rejected"), Strike.TimedFrame(std::numeric_limits<double>::quiet_NaN(), .15, .1, .3), 0);
    TestEqual(TEXT("Unconfigured native atlas rejected"), GyeolPose::FNativeStrike{}.TimedFrame(.15, .15, .1, .3), 0);
    return true;
}

static FString PoseFixture(int32 Views = 1, bool bWalk = false, int32 CellSize = 64, int32 WalkPoses = 4, bool bAttack = false, bool bEntry = false, bool bLocomotionOnly = false, bool bStops = false, bool bStrike = false, bool bStrikeEntry = false, int32 NativeStrikePoses = 0, int32 KickPoses = 0, bool bDash = false)
{
    FString Frames, Tags;
    const TCHAR* Names[] = {TEXT("idle"), TEXT("windup"), TEXT("impact"), TEXT("recover"), TEXT("dash"), TEXT("walk0"), TEXT("walk1"), TEXT("walk2"), TEXT("walk3")};
    const int32 BasePoses = bStrike ? 2 : bLocomotionOnly ? 1 : 5;
    const int32 StrikePoses = NativeStrikePoses > 0 ? NativeStrikePoses : 25;
    const int32 StopPoses = bStops ? 48 : 0;
    const int32 BaseColumns = (bWalk ? BasePoses + WalkPoses : BasePoses) + (bAttack ? 4 : 0) + (bEntry ? 32 : 0) + StopPoses + (bStrike ? StrikePoses : 0) + (bStrikeEntry ? 24 : 0) + KickPoses;
    const int32 Columns = BaseColumns + (bDash ? 280 : 0);
    const TCHAR* AttackNames[] = {TEXT("attack_ready"), TEXT("attack_extend"), TEXT("recover_mid"), TEXT("recover_end")};
    for (int32 I = 0; I < Columns * Views; ++I)
    {
        if (I) { Frames += TEXT(","); Tags += TEXT(","); }
        Frames += FString::Printf(TEXT(R"({"frame":{"x":%d,"y":%d,"w":%d,"h":%d},"sourceSize":{"w":%d,"h":%d},"spriteSourceSize":{"x":0,"y":0,"w":%d,"h":%d},"rotated":false,"trimmed":false,"duration":150})"),
            (I % Columns) * CellSize, (I / Columns) * CellSize, CellSize, CellSize, CellSize, CellSize, CellSize, CellSize);
        const int32 PoseIndex = I % Columns;
        const int32 EntryIndex = PoseIndex - BasePoses - WalkPoses - StopPoses - StrikePoses;
        const FName Pose = bDash && PoseIndex >= BaseColumns
            ? FName(*FString::Printf(TEXT("dash_phase%d_%d"), (PoseIndex - BaseColumns) / 35, (PoseIndex - BaseColumns) % 35 + 1))
            : KickPoses > 0 && PoseIndex >= BaseColumns - KickPoses
            ? FName(*FString::Printf(TEXT("kick%d"), PoseIndex - BaseColumns + KickPoses))
            : bStrikeEntry && EntryIndex >= 0
            ? FName(*FString::Printf(TEXT("jab_entry%d_%d"), EntryIndex / 3, EntryIndex % 3 + 1))
            : bStrike && PoseIndex >= BasePoses + WalkPoses + StopPoses
            ? FName(*FString::Printf(TEXT("jab%d"), PoseIndex - BasePoses - WalkPoses - StopPoses))
            : bStrike && PoseIndex == 1 ? FName(TEXT("dash"))
            : bStops && PoseIndex >= BasePoses + WalkPoses
            ? FName(*FString::Printf(TEXT("stop%d_%d"), (PoseIndex-BasePoses-WalkPoses)/6, (PoseIndex-BasePoses-WalkPoses)%6))
            : PoseIndex < BasePoses ? FName(Names[PoseIndex]) : PoseIndex < BasePoses + WalkPoses
            ? FName(*FString::Printf(TEXT("walk%d"), PoseIndex - BasePoses)) : PoseIndex < 9 + WalkPoses
            ? FName(AttackNames[PoseIndex - 5 - WalkPoses])
            : FName(*FString::Printf(TEXT("%s%d"), (PoseIndex - 9 - WalkPoses) % 2 == 0 ? TEXT("attack_entry") : TEXT("attack_settle"), (PoseIndex - 9 - WalkPoses) / 2));
        const FName Key = GyeolPose::PoseKey(Pose, I / Columns, Views);
        Tags += FString::Printf(TEXT(R"({"name":"%s","from":%d,"to":%d})"), *Key.ToString(), I, I);
    }
    const FString Capability = (bLocomotionOnly ? FString::Printf(TEXT("\"gyeol_locomotion_only\":true,\"gyeol_walk_pose_count\":%d,"), WalkPoses) : FString())
        + (bStops ? TEXT("\"gyeol_stop_transition_version\":1,") : TEXT(""))
        + (bStrike ? NativeStrikePoses > 0 ? FString::Printf(TEXT("\"gyeol_representative_strike_version\":2,\"gyeol_walk_pose_count\":%d,\"gyeol_strike_pose_count\":%d,\"gyeol_strike_fps\":60,\"gyeol_strike_contact_frame\":30,\"gyeol_strike_active_end_frame\":46,"), WalkPoses, NativeStrikePoses)
            : FString(TEXT("\"gyeol_representative_strike_version\":1,\"gyeol_walk_pose_count\":48,")) : FString())
        + (bStrikeEntry ? TEXT("\"gyeol_representative_strike_entry_version\":1,") : TEXT(""))
        + (KickPoses > 0 ? FString::Printf(TEXT("\"gyeol_kick_pose_count\":%d,\"gyeol_kick_contact_frame\":39,\"gyeol_kick_active_end_frame\":51,"), KickPoses) : FString())
        + (bDash ? TEXT("\"gyeol_dash_version\":1,\"gyeol_dash_fps\":60,\"gyeol_dash_entry_end_frame\":12,\"gyeol_dash_hold_end_frame\":24,\"gyeol_dash_end_frame\":36,\"gyeol_dash_bucket_count\":8,") : TEXT(""));
    return TEXT("{\"frames\":[") + Frames + TEXT("],\"meta\":{") + Capability + FString::Printf(TEXT("\"size\":{\"w\":%d,\"h\":%d},\"frameTags\":["), Columns * CellSize, Views * CellSize) + Tags
        + FString::Printf(TEXT(R"(],"slices":[{"name":"foot","keys":[{"frame":0,"bounds":{"x":0,"y":0,"w":%d,"h":%d},"pivot":{"x":%d,"y":%d}}]}]}})"), CellSize, CellSize, CellSize / 2, CellSize * 7 / 8);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanPhasedDashAtlasTest, "Gyeol.Art.DanPhasedDashAtlas",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanPhasedDashAtlasTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Legacy = PoseFixture(8, true, 32, 48, false, false, false, true, true, true);
    TestTrue(TEXT("Legacy atlas still accepted"), GyeolPose::Parse(Legacy, Atlas));
    TestFalse(TEXT("Legacy has no implicit dash clip"), Atlas.bPhasedDash);
    const FString Valid = PoseFixture(8, true, 32, 48, false, false, false, true, true, true, 0, 0, true);
    TestTrue(TEXT("Versioned 3416-cell atlas accepted"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("Phased dash enabled explicitly"), Atlas.bPhasedDash);
    TestEqual(TEXT("All old and new cells retained"), Atlas.Frames.Num(), 3416);
    TestTrue(TEXT("Native strike variant accepted"), GyeolPose::Parse(
        PoseFixture(8, true, 32, 48, false, false, false, false, true, false, 104, 0, true), Atlas));
    for (const auto& Pair : TArray<TPair<FString, FString>>{
        {TEXT("\"gyeol_dash_version\":1"), TEXT("\"gyeol_dash_version\":2")},
        {TEXT("\"gyeol_dash_fps\":60,"), TEXT("")},
        {TEXT("\"gyeol_dash_fps\":60"), TEXT("\"gyeol_dash_fps\":\"60\"")},
        {TEXT("\"gyeol_dash_entry_end_frame\":12"), TEXT("\"gyeol_dash_entry_end_frame\":13")},
        {TEXT("\"gyeol_dash_hold_end_frame\":24"), TEXT("\"gyeol_dash_hold_end_frame\":25")},
        {TEXT("\"gyeol_dash_end_frame\":36"), TEXT("\"gyeol_dash_end_frame\":35")},
        {TEXT("\"gyeol_dash_bucket_count\":8"), TEXT("\"gyeol_dash_bucket_count\":7")},
        {TEXT("dash_phase7_35.se"), TEXT("missing.se")},
        {TEXT("dash_phase0_12.e"), TEXT("dash_phase0_24.e")},
        {TEXT("dash.e"), TEXT("missing-legacy.e")}})
    {
        TestFalse(TEXT("Malformed phase contract rejected"), GyeolPose::Parse(Valid.Replace(*Pair.Key, *Pair.Value), Atlas));
        TestFalse(TEXT("Failed parse clears capability"), Atlas.bPhasedDash);
    }
    TestFalse(TEXT("Metadata cannot authorize absent cells"), GyeolPose::Parse(
        Legacy.Replace(TEXT("\"meta\":{"), TEXT("\"meta\":{\"gyeol_dash_version\":1,")), Atlas));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanPhasedDashTimingTest, "Gyeol.Art.DanPhasedDashTiming",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanPhasedDashTimingTest::RunTest(const FString& Parameters)
{
    for (int32 Bucket = 0; Bucket < 8; ++Bucket)
    {
        GyeolPose::FDashPlayback Dash;
        TestEqual(TEXT("Entry starts at shared run drawing"), Dash.Sample(true, false, 1, 0, 10, .15, Bucket * 6), 0);
        TestEqual(TEXT("Entry phase captured once"), Dash.Bucket, Bucket);
        TestEqual(TEXT("Entry endpoint name"), Dash.Pose(0), FName(*FString::Printf(TEXT("walk%d"), Bucket * 6)));
        TestEqual(TEXT("Hold starts halfway through gameplay dash"), Dash.Sample(true, false, 1, .075, 10.075, .15, 0), 12);
        TestEqual(TEXT("Run return endpoint"), Dash.ReturnPhase(), (Bucket * 6 + 36) % 48);
        TestEqual(TEXT("Visual recovery starts at actual action end"), Dash.Sample(false, true, 1, 0, 10.15, .15, 0), 24);
        TestEqual(TEXT("Pause keeps recovery drawing"), Dash.Sample(false, true, 1, 0, 10.15, .15, 0), 24);
        TestEqual(TEXT("Recovery advances at authored 60Hz"), Dash.Sample(false, true, 1, 0, 10.25, .15, 0), 30);
        TestEqual(TEXT("Recovery never extends beyond 0.2 seconds"), Dash.Sample(false, true, 1, 0, 10.35, .15, 0), INDEX_NONE);
        TestEqual(TEXT("Expired recovery cannot restart"), Dash.Sample(false, true, 1, 0, 10.36, .15, 0), INDEX_NONE);
    }
    for (int32 Rate : {30, 60, 120, 240})
    {
        GyeolPose::FDashPlayback Dash;
        for (int32 I = 0; I <= Rate; ++I)
        {
            const double Time = double(I) / Rate;
            const bool Active = Time < .15;
            const int32 Frame = Dash.Sample(Active, !Active, 5, Active ? Time : 0, Time, .15, 12);
            const int32 Expected = Active ? FMath::FloorToInt(Time / .15 * 24 + 1.e-7)
                : Time < .35 - 1.e-7 ? 24 + FMath::FloorToInt((Time - .15) * 60 + 1.e-7) : INDEX_NONE;
            TestEqual(TEXT("Elapsed clock is frame-rate independent"), Frame, Expected);
        }
    }
    GyeolPose::FDashPlayback Dash;
    for (int32 Phase = 0; Phase < 48; ++Phase)
    {
        Dash.Reset();
        Dash.Sample(true, false, 1, 0, 0, .15, Phase);
        TestEqual(TEXT("Arbitrary run phase chooses nearest authored bucket"), Dash.Bucket, ((Phase + 3) / 6) % 8);
        Dash.Sample(false, true, 1, 0, .25, .15, Phase);
        TestEqual(TEXT("Partial recovery exposes current source phase"), Dash.RecoveryTick, 30);
    }
    Dash.Reset();
    TestEqual(TEXT("Skipped dash does not fabricate recovery"), Dash.Sample(false, true, 7, 0, 1, .15, 0), INDEX_NONE);
    Dash.Sample(true, false, 8, .1, 1.1, .15, 47);
    TestEqual(TEXT("Nearest run phase wraps"), Dash.Bucket, 0);
    TestEqual(TEXT("Hit or action interrupts visual recovery"), Dash.Sample(false, false, 9, 0, 1.16, .15, 0), INDEX_NONE);
    Dash.Sample(true, false, 10, .1, 2.1, .15, 12);
    TestEqual(TEXT("Different neutral instance cannot recover stale dash"), Dash.Sample(false, true, 11, 0, 2.16, .15, 0), INDEX_NONE);
    Dash.Sample(true, false, 12, .1, 3.1, .15, 12);
    TestEqual(TEXT("Clock reset cancels visual state"), Dash.Sample(false, true, 12, 0, 0, .15, 0), INDEX_NONE);
    Dash.Sample(true, false, 13, .1, 4.1, .15, 12);
    Dash.Reset();
    TestEqual(TEXT("Room/fighter reset cancels visual state"), Dash.Sample(false, true, 13, 0, 4.16, .15, 0), INDEX_NONE);
    for (double Bad : {-1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        TestEqual(TEXT("Invalid elapsed rejected"), Dash.Sample(true, false, 14, Bad, 1, .15, 0), INDEX_NONE);
        TestEqual(TEXT("Invalid clock rejected"), Dash.Sample(true, false, 14, 0, Bad, .15, 0), INDEX_NONE);
        TestEqual(TEXT("Invalid duration rejected"), Dash.Sample(true, false, 14, 0, 1, Bad, 0), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanNativeStrikeTest, "Gyeol.Art.DanNativeStrike",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanNativeStrikeTest::RunTest(const FString& Parameters)
{
    for (int32 Count : {4, 104, 240})
    {
        const auto Schedule = GyeolCombat::FStrikeCaptureSchedule::Make(Count, false);
        TestTrue(TEXT("Capture finishes attack plus pause before retreat"), Schedule.RetreatFrame > 20 + Count + 10);
        TestTrue(TEXT("Capture includes stationary guard after retreat"), Schedule.SegmentFrames > Schedule.RetreatFrame + 20);
        TestEqual(TEXT("Four complete directions"), Schedule.TotalFrames(), (Count + 80) * 4);
    }
    TestEqual(TEXT("Legacy capture unchanged"), GyeolCombat::FStrikeCaptureSchedule::Make(0, false).TotalFrames(), 320);
    TestEqual(TEXT("Legacy sweep unchanged"), GyeolCombat::FStrikeCaptureSchedule::Make(0, true).TotalFrames(), 880);
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 64, 44, false, false, false, false, true, false, 104);
    TestTrue(TEXT("Native 44/104 contract loads"), GyeolPose::Parse(Valid, Atlas));
    TestEqual(TEXT("Run count retained"), Atlas.WalkPoseCount, 44);
    TestEqual(TEXT("Strike count retained"), Atlas.NativeStrike.PoseCount, 104);
    TestFalse(TEXT("No implicit stop blend"), Atlas.bStopTransitions);
    TestFalse(TEXT("No legacy movement entry"), Atlas.bMovementEntry);
    for (int32 Frame = 0; Frame < 104; ++Frame)
    {
        TestEqual(TEXT("One drawing per native 60Hz tick"), Atlas.NativeStrike.Frame(Frame / 60.0), Frame);
    }
    TestEqual(TEXT("Whole clip is not shortened"), Atlas.NativeStrike.Frame(1.7), 102);
    TestEqual(TEXT("Bounded after end"), Atlas.NativeStrike.Frame(1.e20), 103);
    TestEqual(TEXT("Negative time safe"), Atlas.NativeStrike.Frame(-1), 0);
    TestEqual(TEXT("Contact pose"), Atlas.NativeStrike.Pose(.5), FName(TEXT("jab30")));
    const FString TwoStrike = PoseFixture(8, true, 64, 42, false, false, false, false, true, false, 104, 82);
    TestTrue(TEXT("Native jab plus kick loads"), GyeolPose::Parse(TwoStrike, Atlas));
    TestEqual(TEXT("Kick count retained"), Atlas.NativeKick.PoseCount, 82);
    TestEqual(TEXT("Kick contact follows gameplay timing"), Atlas.NativeKick.TimedFrame(14.0/60,14.0/60,7.0/60,21.0/60),39);
    TestFalse(TEXT("Missing kick direction rejected"), GyeolPose::Parse(TwoStrike.Replace(TEXT("kick81.e"),TEXT("absent.e")),Atlas));
    TestFalse(TEXT("Malformed kick timing rejected"), GyeolPose::Parse(TwoStrike.Replace(TEXT("\"gyeol_kick_contact_frame\":39"),TEXT("\"gyeol_kick_contact_frame\":51")),Atlas));
    for (const auto& Pair : TArray<TPair<FString, FString>>{
        {TEXT("\"gyeol_strike_fps\":60"), TEXT("\"gyeol_strike_fps\":30")},
        {TEXT("\"gyeol_strike_contact_frame\":30"), TEXT("\"gyeol_strike_contact_frame\":0")},
        {TEXT("\"gyeol_strike_active_end_frame\":46"), TEXT("\"gyeol_strike_active_end_frame\":30")},
        {TEXT("\"gyeol_strike_active_end_frame\":46"), TEXT("\"gyeol_strike_active_end_frame\":104")},
        {TEXT("\"gyeol_strike_pose_count\":104"), TEXT("\"gyeol_strike_pose_count\":241")},
        {TEXT("jab103.e"), TEXT("missing.e")}})
    {
        TestFalse(TEXT("Bad native contract rejected"), GyeolPose::Parse(Valid.Replace(*Pair.Key, *Pair.Value), Atlas));
        TestFalse(TEXT("Failure clears native capability"), Atlas.NativeStrike.IsValid());
    }
    TestFalse(TEXT("Old stop clips cannot silently mix with native timing"),
        GyeolPose::Parse(PoseFixture(8, true, 64, 44, false, false, false, true, true, false, 104), Atlas));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContactAnchorTest, "Gyeol.Art.ContactAnchor",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FContactAnchorTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Base = PoseFixture();
    const auto WithAnchor = [&Base](const FString& Value)
    { return Base.Replace(TEXT("\"meta\":{"), *(TEXT("\"meta\":{\"gyeol_contact_anchors\":") + Value + TEXT(","))); };
    TestTrue(TEXT("Legacy atlas has no anchors"), GyeolPose::Parse(Base, Atlas) && Atlas.ContactAnchors.IsEmpty());
    TestTrue(TEXT("Optional fractional marker loads"), GyeolPose::Parse(
        WithAnchor(TEXT(R"({"impact":{"x":40.5,"y":12.25}})")), Atlas));
    TestTrue(TEXT("Marker preserved"), Atlas.ContactAnchors.Contains(TEXT("impact"))
        && Atlas.ContactAnchors[TEXT("impact")].Equals(FVector2D(40.5, 12.25), 1.e-8));
    for (const TCHAR* Bad : {TEXT("null"), TEXT("{}"), TEXT(R"({"missing":{"x":1,"y":2}})"),
        TEXT(R"({"impact":{"x":64,"y":2}})"), TEXT(R"({"impact":{"x":-1,"y":2}})"),
        TEXT(R"({"impact":{"x":1,"y":null}})")})
    { TestFalse(TEXT("Invalid marker rejected"), GyeolPose::Parse(WithAnchor(Bad), Atlas)); }
    TestTrue(TEXT("Failure clears markers"), Atlas.ContactAnchors.IsEmpty());
    for (const double Density : {1.0, 2.0, 3.25})
    {
        const FRotator View(-30, 45, 0);
        const FRotationMatrix Basis(View);
        const FVector Offset = GyeolPose::ContactOffset(FVector2D(40.5, 12.25), FIntPoint(32, 56), Density, View);
        TestTrue(TEXT("Marker screen X"), FMath::IsNearlyEqual(FVector::DotProduct(Offset, Basis.GetUnitAxis(EAxis::Y)), 8.5*Density, 1.e-6));
        TestTrue(TEXT("Marker screen Y"), FMath::IsNearlyEqual(FVector::DotProduct(Offset, Basis.GetUnitAxis(EAxis::Z)), 43.75*Density, 1.e-6));
        const FVector Front(75, 20, 180), Forward = View.Vector();
        const FVector Projected = GyeolPose::ContactOnFrontPlane(Offset, Front, Forward);
        TestTrue(TEXT("Depth projection preserves screen position"),
            FVector::CrossProduct(Projected-Offset, Forward).Size() < 1.e-6);
        TestTrue(TEXT("Effect clears target front depth"), FMath::Abs(FVector::DotProduct(Projected-Front, Forward)) < 1.e-6);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanStrikeEntryTest, "Gyeol.Art.DanStrikeEntry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanStrikeEntryTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 64, 48, false, false, false, true, true, true);
    TestTrue(TEXT("1176 explicit entry cells load"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("New entry capability"), Atlas.bRepresentativeStrikeEntry);
    for (const TCHAR* Name : {TEXT("jab_entry0_1.e"), TEXT("jab_entry7_3.se"), TEXT("jab5.n")})
    { TestFalse(TEXT("All entries and contact mandatory"), GyeolPose::Parse(Valid.Replace(Name, TEXT("unknown")), Atlas)); }
    TestFalse(TEXT("Failed parse clears entry"), Atlas.bRepresentativeStrikeEntry);
    TestFalse(TEXT("Version enforced"), GyeolPose::Parse(Valid.Replace(
        TEXT("\"gyeol_representative_strike_entry_version\":1"), TEXT("\"gyeol_representative_strike_entry_version\":2")), Atlas));
    TestFalse(TEXT("Entry requires strike capability"), GyeolPose::Parse(Valid.Replace(
        TEXT("\"gyeol_representative_strike_version\":1,"), TEXT("")), Atlas));
    TestFalse(TEXT("Undeclared entry cells rejected"), GyeolPose::Parse(Valid.Replace(
        TEXT("\"gyeol_representative_strike_entry_version\":1,"), TEXT("")), Atlas));
    TestTrue(TEXT("Prior strike still loads"), GyeolPose::Parse(
        PoseFixture(8, true, 64, 48, false, false, false, true, true), Atlas));
    TestFalse(TEXT("Prior strike has no entry"), Atlas.bRepresentativeStrikeEntry);
    GyeolPose::FAttackEntry Entry;
    for (int32 Phase = 0; Phase < 48; ++Phase)
    {
        Entry.Observe(true, Phase + 1, Phase, 48, Phase % 8);
        Entry.Observe(true, Phase + 1, INDEX_NONE, 48, (Phase + 1) % 8);
        TestEqual(TEXT("All phases retained across gait reset"), Entry.Phase, Phase);
        TestEqual(TEXT("Prior displayed heading retained"), Entry.FacingSector, Phase % 8);
        for (int32 Frame = 0; Frame <= 24; ++Frame)
        {
            const FName Expected = Frame == 0 ? FName(*FString::Printf(TEXT("walk%d"), Phase))
                : Frame < 4 ? FName(*FString::Printf(TEXT("jab_entry%d_%d"), ((Phase + 3) / 6) % 8, Frame))
                : FName(*FString::Printf(TEXT("jab%d"), Frame));
            TestEqual(TEXT("Entry joins original frame4 before impact"),
                GyeolPose::RepresentativeStrikeEntryPose(Frame, Entry.Phase), Expected);
        }
    }
    Entry.Observe(false, 49, 0, 48, 0);
    TestEqual(TEXT("Cancel resets phase"), Entry.Phase, INDEX_NONE);
    TestEqual(TEXT("Cancel resets heading"), Entry.FacingSector, INDEX_NONE);
    Entry.Observe(true, 50, 48, 48, 8);
    TestEqual(TEXT("Out-of-range phase rejected"), Entry.Phase, INDEX_NONE);
    TestEqual(TEXT("Out-of-range heading rejected"), Entry.FacingSector, INDEX_NONE);
    TestEqual(TEXT("Standing entry unchanged"), GyeolPose::RepresentativeStrikeEntryPose(0, INDEX_NONE), FName(TEXT("jab0")));
    const FRotator View(-30, 45, 0);
    for (int32 From = 0; From < 8; ++From)
    {
        for (int32 To = 0; To < 8; ++To)
        {
            const double Angle = To * UE_PI / 4;
            const FVector Target = GyeolPose::GroundFromScreen(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)), View);
            TestEqual(TEXT("Entry starts at prior orientation"), GyeolPose::ScreenDirection(
                GyeolPose::StrikeEntryDirection(0, From, Target, View), View), From);
            TestTrue(TEXT("Heading rejoins exact target before impact"),
                GyeolPose::StrikeEntryDirection(4, From, Target, View).Equals(Target, 1.e-10));
            for (int32 Frame = 1; Frame < 4; ++Frame)
            {
                const int32 Delta = (To - From + 12) % 8 - 4;
                const double T = Frame / 4.0;
                const int32 Expected = (FMath::FloorToInt(From + Delta * T * T * (3 - 2 * T) + .5) + 8) % 8;
                TestEqual(TEXT("Shortest arc including wrap and half turn"), GyeolPose::ScreenDirection(
                    GyeolPose::StrikeEntryDirection(Frame, From, Target, View), View), Expected);
            }
        }
    }
    for (int32 Hz : {30, 60, 120})
    {
        for (int32 Tick = 0; Tick <= Hz; ++Tick)
        {
            const double T = Tick / double(Hz);
            const int32 Frame = GyeolPose::RepresentativeStrikeFrame(T, 5./60, 4./60, 15./60, false);
            if (T >= 4./60)
            {
                TestEqual(TEXT("Post-entry timing and drawings untouched"), GyeolPose::RepresentativeStrikeEntryPose(Frame, 47),
                    GyeolPose::RepresentativeStrikePose(T, 5./60, 4./60, 15./60, false));
            }
        }
    }
    TestEqual(TEXT("Impact freeze bypasses entry"), GyeolPose::RepresentativeStrikeEntryPose(
        GyeolPose::RepresentativeStrikeFrame(.12, 5./60, 4./60, 15./60, true), 47), FName(TEXT("jab5")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanRepresentativeStrikeTest, "Gyeol.Art.DanRepresentativeStrike",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanRepresentativeStrikeTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 64, 48, false, false, false, true, true);
    TestTrue(TEXT("984 explicit strike cells accepted"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("Strike and settling capabilities"), Atlas.bRepresentativeStrike && Atlas.bStopTransitions);
    TestEqual(TEXT("48 gait poses retained"), Atlas.WalkPoseCount, 48);
    TestFalse(TEXT("No legacy attack or locomotion-only implication"),
        Atlas.bAttackTransitions || Atlas.bMovementEntry || Atlas.bLocomotionOnly);
    for (const TCHAR* Name : {TEXT("jab0.e"), TEXT("jab24.se"), TEXT("dash.n"), TEXT("stop7_5.w")})
    { TestFalse(TEXT("Required strike pose enforced"), GyeolPose::Parse(Valid.Replace(Name, TEXT("unknown")), Atlas)); }
    TestFalse(TEXT("Unknown version rejected"), GyeolPose::Parse(Valid.Replace(
        TEXT("\"gyeol_representative_strike_version\":1"), TEXT("\"gyeol_representative_strike_version\":2")), Atlas));
    TestFalse(TEXT("Exclusive capability enforced"), GyeolPose::Parse(Valid.Replace(
        TEXT("\"gyeol_representative_strike_version\":1,"), TEXT("\"gyeol_representative_strike_version\":1,\"gyeol_locomotion_only\":true,")), Atlas));
    for (int32 Frame = 0; Frame <= 24; ++Frame)
    {
        TestEqual(TEXT("Default gameplay time selects matching drawing"),
            GyeolPose::RepresentativeStrikePose(Frame / 60.0, 5./60, 4./60, 15./60, false),
            FName(*FString::Printf(TEXT("jab%d"), Frame)));
    }
    TestEqual(TEXT("Impact pause holds contact"), GyeolPose::RepresentativeStrikePose(.12, 5./60, 4./60, 15./60, true), FName(TEXT("jab5")));
    TestEqual(TEXT("Scaled timing still contacts at active start"),
        GyeolPose::RepresentativeStrikePose(.2, .2, .1, .3, false), FName(TEXT("jab5")));
    TestEqual(TEXT("Hitch clamps without integer overflow"),
        GyeolPose::RepresentativeStrikePose(1.e20, .2, .1, .3, false), FName(TEXT("jab24")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanStopStrikeEntryTest, "Gyeol.Art.DanStopStrikeEntry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanStopStrikeEntryTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAttackEntry Entry;
    for (int32 Bucket = 0; Bucket < 8; ++Bucket)
    {
        for (int32 Step = 0; Step < 6; ++Step)
        {
            const uint64 Instance = 1 + Bucket * 6 + Step;
            Entry.Observe(true, Instance, INDEX_NONE, 48, Bucket, Bucket, Step);
            Entry.Observe(true, Instance, INDEX_NONE, 48, 0);
            TestEqual(TEXT("Captured stop survives sprite reset"), Entry.StopBucket, Bucket);
            TestEqual(TEXT("Captured sample is stable"), Entry.StopStep, Step);
            TestEqual(TEXT("Only first two stop samples reuse run entry"), Entry.Phase, Step < 2 ? Bucket * 6 : INDEX_NONE);
            TestEqual(TEXT("Original heading retained"), Entry.FacingSector, Bucket);
            for (int32 Frame = 0; Frame <= 24; ++Frame)
            {
                const FName Expected = Frame == 0 ? FName(*FString::Printf(TEXT("stop%d_%d"), Bucket, Step))
                    : Step < 2 && Frame < 4 ? FName(*FString::Printf(TEXT("jab_entry%d_%d"), Bucket, Frame))
                    : FName(*FString::Printf(TEXT("jab%d"), Frame));
                TestEqual(TEXT("Stop entry joins unchanged jab before impact"),
                    GyeolPose::RepresentativeStrikeEntryPose(Frame, Entry.Phase, Entry.StopBucket, Entry.StopStep), Expected);
            }
        }
    }
    Entry.Observe(false, 48, INDEX_NONE, 48, 0, 7, 1);
    TestEqual(TEXT("Cancellation clears stop bucket"), Entry.StopBucket, INDEX_NONE);
    TestEqual(TEXT("Cancellation clears stop sample"), Entry.StopStep, INDEX_NONE);
    uint64 Instance = 50;
    for (FIntPoint Invalid : {FIntPoint(-1, 0), FIntPoint(8, 0), FIntPoint(0, -1), FIntPoint(0, 6)})
    {
        Entry.Observe(true, Instance++, INDEX_NONE, 48, 0, Invalid.X, Invalid.Y);
        TestEqual(TEXT("Invalid stop rejected"), Entry.StopBucket, INDEX_NONE);
        TestEqual(TEXT("Invalid stop cannot invent a walk entry"), Entry.Phase, INDEX_NONE);
    }
    Entry.Observe(true, Instance++, INDEX_NONE, 16, 0, 0, 0);
    TestEqual(TEXT("Legacy gait does not select stop entry"), Entry.StopBucket, INDEX_NONE);
    Entry.Observe(true, Instance++, 47, 48, 0, 2, 0);
    TestEqual(TEXT("Walking takes precedence"), Entry.Phase, 47);
    TestEqual(TEXT("Walking discards stale stop"), Entry.StopBucket, INDEX_NONE);
    Entry.Observe(true, Instance++, INDEX_NONE, 48, 0, 2, 0);
    TestEqual(TEXT("Impact pause bypasses all entry poses"), GyeolPose::RepresentativeStrikeEntryPose(
        GyeolPose::RepresentativeStrikeFrame(.12, 5./60, 4./60, 15./60, true), Entry.Phase, Entry.StopBucket, Entry.StopStep),
        FName(TEXT("jab5")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanStopTransitionTest, "Gyeol.Art.DanStopTransition",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanStopTransitionTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 64, 48, false, false, true, true);
    TestTrue(TEXT("Explicit 776-cell settling atlas loads"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("Stop capability retained"), Atlas.bStopTransitions);
    TestFalse(TEXT("Missing settling pose rejected"), GyeolPose::Parse(Valid.Replace(TEXT("stop7_5.se"), TEXT("unknown")), Atlas));
    TestFalse(TEXT("Failure clears capability"), Atlas.bStopTransitions);
    TestFalse(TEXT("Unknown version rejected"), GyeolPose::Parse(Valid.Replace(TEXT("\"gyeol_stop_transition_version\":1"), TEXT("\"gyeol_stop_transition_version\":2")), Atlas));
    TestFalse(TEXT("No undeclared settling poses"), GyeolPose::Parse(Valid.Replace(TEXT("\"gyeol_stop_transition_version\":1,"), TEXT("")), Atlas));
    TestTrue(TEXT("Old locomotion still accepted"), GyeolPose::Parse(PoseFixture(8, true, 256, 48, false, false, true), Atlas));
    TestFalse(TEXT("Old art does not gain settling"), Atlas.bStopTransitions);
    for (int32 Phase = 0; Phase < 48; ++Phase)
    {
        GyeolPose::FLocomotionStop Stop;
        TestEqual(TEXT("Initial rest does not invent stop"), Stop.Sample(INDEX_NONE, 1./60), FName(TEXT("idle")));
        TestEqual(TEXT("No displayed stop at rest"), Stop.GetDisplayedStep(), INDEX_NONE);
        Stop.Sample(Phase, 1./60);
        TestEqual(TEXT("No displayed stop while running"), Stop.GetDisplayedStep(), INDEX_NONE);
        const int32 Bucket = ((Phase+3)/6)%8;
        for (int32 Step = 0; Step < 6; ++Step)
        {
            TestEqual(TEXT("Six exact settling samples"), Stop.Sample(INDEX_NONE, 1./60),
                FName(*FString::Printf(TEXT("stop%d_%d"), Bucket, Step)));
            TestEqual(TEXT("Exposed sample matches drawn stop"), Stop.GetDisplayedStep(), Step);
        }
        TestEqual(TEXT("Completes without additional movement"), Stop.Sample(INDEX_NONE, 1./60), FName(TEXT("idle")));
        TestEqual(TEXT("Expired stop cannot be captured"), Stop.GetDisplayedStep(), INDEX_NONE);
        Stop.Sample(Phase, 1./60);
        Stop.Sample(INDEX_NONE, 1./60);
        TestEqual(TEXT("Movement interrupts settling immediately"), Stop.Sample(12, 1./60), FName(TEXT("walk12")));
        Stop.Reset();
        TestEqual(TEXT("Reset clears displayed sample"), Stop.GetDisplayedStep(), INDEX_NONE);
        TestEqual(TEXT("Action reset discards queued settling"), Stop.Sample(INDEX_NONE, 1./60), FName(TEXT("idle")));
    }
    for (int32 Hz : {30, 60, 120})
    {
        GyeolPose::FLocomotionStop Stop;
        Stop.Sample(47, 1./Hz);
        Stop.Sample(INDEX_NONE, 1./Hz);
        for (int32 I = 1; I < Hz/10; ++I) { Stop.Sample(INDEX_NONE, 1./Hz); }
        TestEqual(TEXT("Same 100ms visual duration at each tick rate"), Stop.Sample(INDEX_NONE, 1./Hz), FName(TEXT("idle")));
    }
    GyeolPose::FLocomotionStop Hitch;
    Hitch.Sample(0, 0);
    Hitch.Sample(INDEX_NONE, 0);
    TestEqual(TEXT("Negative time does not rewind"), Hitch.Sample(INDEX_NONE, -1), FName(TEXT("stop0_0")));
    TestEqual(TEXT("Large hitch cannot overflow a pose index"), Hitch.Sample(INDEX_NONE, 1.e20), FName(TEXT("idle")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDanLocomotionAtlasTest, "Gyeol.Art.DanLocomotionAtlas",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDanLocomotionAtlasTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    for (const int32 Count : {4, 44, 60, 120})
    {
        const FString Candidate = PoseFixture(8, true, 64, Count, false, false, true);
        TestTrue(TEXT("Variable-length AI gait loads"), GyeolPose::Parse(Candidate, Atlas));
        TestEqual(TEXT("AI pose count preserved"), Atlas.WalkPoseCount, Count);
        TestFalse(TEXT("Every direction requires final pose"), GyeolPose::Parse(Candidate.Replace(
            *FString::Printf(TEXT("walk%d.se"), Count - 1), TEXT("unknown")), Atlas));
    }
    for (const int32 Count : {3, 121})
    { TestFalse(TEXT("Out-of-range gait rejected"), GyeolPose::Parse(PoseFixture(8, true, 64, Count, false, false, true), Atlas)); }
    TestFalse(TEXT("48-phase stop clips cannot attach to 44-phase AI run"),
        GyeolPose::Parse(PoseFixture(8, true, 64, 44, false, false, true, true), Atlas));
    double Cycle;
    const FString Timing = TEXT(R"({"locomotion":{"pose_count":44,"cycle_cm":228.65,"fps":60}})");
    TestTrue(TEXT("AI sample rate loads"), GyeolPose::ParseLocomotion(Timing, 44, Cycle, true));
    TestFalse(TEXT("30Hz data cannot silently play at 60Hz"), GyeolPose::ParseLocomotion(
        Timing.Replace(TEXT("\"fps\":60"), TEXT("\"fps\":30")), 44, Cycle, true));
    TestFalse(TEXT("Study cannot use legacy timing fallback"), GyeolPose::ParseLocomotion(TEXT("{}"), 4, Cycle, true));
    const FString Valid = PoseFixture(8, true, 256, 48, false, false, true);
    TestTrue(TEXT("Dedicated 392-cell locomotion atlas loads"), GyeolPose::Parse(Valid, Atlas));
    TestEqual(TEXT("All 48 run poses retained"), Atlas.WalkPoseCount, 48);
    TestTrue(TEXT("Explicit locomotion-only capability"), Atlas.bLocomotionOnly);
    TestFalse(TEXT("Does not invent attack frames"), Atlas.bAttackTransitions || Atlas.bMovementEntry);
    for (const TCHAR* Name : {TEXT("idle.e"), TEXT("walk47.se"), TEXT("walk0.n")})
    { TestFalse(TEXT("Incomplete locomotion atlas rejected"), GyeolPose::Parse(Valid.Replace(Name, TEXT("unknown")), Atlas)); }
    for (const TCHAR* Count : {TEXT("47"), TEXT("48.5"), TEXT("true")})
    { TestFalse(TEXT("Invalid declared pose count rejected"), GyeolPose::Parse(Valid.Replace(TEXT("\"gyeol_walk_pose_count\":48"), *(FString(TEXT("\"gyeol_walk_pose_count\":"))+Count)), Atlas)); }
    TestFalse(TEXT("False capability is not silently ignored"), GyeolPose::Parse(Valid.Replace(TEXT("\"gyeol_locomotion_only\":true"), TEXT("\"gyeol_locomotion_only\":false")), Atlas));
    TestFalse(TEXT("Failure clears capability"), Atlas.bLocomotionOnly);
    TestTrue(TEXT("Legacy combat atlas remains accepted"), GyeolPose::Parse(PoseFixture(8, true, 96, 16, true), Atlas));
    TestFalse(TEXT("Legacy atlas does not become locomotion-only"), Atlas.bLocomotionOnly);
    GyeolPose::FStrideClock Gait;
    Gait.CycleCm = 240;
    Gait.PoseCount = 48;
    Gait.Sample(FVector::ZeroVector, true);
    TSet<int32> Seen;
    for (int32 I = 1; I <= 96; ++I) { Seen.Add(Gait.Sample(FVector(I*2.5, 0, 0), true)); }
    TestEqual(TEXT("Distance clock addresses all 48 phases"), Seen.Num(), 48);
    TestEqual(TEXT("Stopped locomotion returns idle"), Gait.Sample(FVector(240, 0, 0), true), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementAttackEntryTest, "Gyeol.Art.MovementAttackEntry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FMovementAttackEntryTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 96, 16, true, true);
    TestTrue(TEXT("456-cell entry atlas loads"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("Entry poses explicitly present"), Atlas.bMovementEntry);
    TestEqual(TEXT("Still sixteen gait phases"), Atlas.WalkPoseCount, 16);
    for (const TCHAR* Name : {TEXT("attack_entry0.e"), TEXT("attack_settle15.se")})
    { TestFalse(TEXT("Incomplete entries rejected"), GyeolPose::Parse(Valid.Replace(Name, TEXT("unknown")), Atlas)); }
    TestFalse(TEXT("Failed load clears capability"), Atlas.bMovementEntry);
    TestTrue(TEXT("200-cell atlas retained"), GyeolPose::Parse(PoseFixture(8, true, 96, 16, true), Atlas));
    TestFalse(TEXT("Older art cannot select missing entries"), Atlas.bMovementEntry);
    GyeolPose::FAttackEntry Entry;
    for (int32 Phase = 0; Phase < 16; ++Phase)
    {
        Entry.Observe(true, Phase + 1, Phase);
        TestEqual(TEXT("New action captures last drawn phase"), Entry.Phase, Phase);
        Entry.Observe(true, Phase + 1, INDEX_NONE);
        TestEqual(TEXT("Action holds captured phase after gait reset"), Entry.Phase, Phase);
        const double Startup = 5.0/60, Active = 4.0/60, Recovery = 15.0/60;
        const auto Pose = [&](double T, bool Enabled = true, int32 P = -2)
        { return GyeolPose::MovementAttackPose(T, Startup, Active, Recovery, true, Enabled, P == -2 ? Phase : P); };
        TestEqual(TEXT("Immediate entry drawing"), Pose(0), FName(*FString::Printf(TEXT("attack_entry%d"), Phase)));
        TestEqual(TEXT("Settling drawing"), Pose(Startup/2), FName(*FString::Printf(TEXT("attack_settle%d"), Phase)));
        TestEqual(TEXT("Last anticipation segment unchanged"), Pose(Startup*2/3), FName(TEXT("attack_extend")));
        TestEqual(TEXT("Idle falls back"), Pose(0, true, INDEX_NONE), FName(TEXT("attack_ready")));
        TestEqual(TEXT("Old atlas falls back"), Pose(0, false), FName(TEXT("attack_ready")));
        for (int32 Hz : {30, 60, 120})
        {
            for (int32 Frame = 0; Frame < Hz*.4; ++Frame)
            {
                const double T = Frame/double(Hz);
                TestEqual(TEXT("Hit window unchanged at each frame rate"), Pose(T) == FName(TEXT("impact")), T >= Startup && T < Startup+Active);
            }
        }
    }
    Entry.Observe(false, 17, 4);
    TestEqual(TEXT("Cancel discards entry"), Entry.Phase, INDEX_NONE);
    Entry.Observe(true, 18, INDEX_NONE);
    TestEqual(TEXT("Later stationary attack does not reuse gait"), Entry.Phase, INDEX_NONE);
    Entry.Observe(true, 19, 16);
    TestEqual(TEXT("Invalid phase cannot address an atlas"), Entry.Phase, INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackTransitionAtlasTest, "Gyeol.Art.AttackTransitionAtlas",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FAttackTransitionAtlasTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 96, 16, true);
    TestTrue(TEXT("200 cells load with running and attack transitions"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("Attack capability is explicit"), Atlas.bAttackTransitions);
    TestEqual(TEXT("Extra attack poses are not walk phases"), Atlas.WalkPoseCount, 16);
    for (const TCHAR* Name : {TEXT("attack_ready.n"), TEXT("attack_extend.e"), TEXT("recover_mid.s"), TEXT("recover_end.se")})
    { TestFalse(TEXT("Each added pose is required"), GyeolPose::Parse(Valid.Replace(Name, TEXT("unknown")), Atlas)); }
    TestFalse(TEXT("Failure clears transition capability"), Atlas.bAttackTransitions);
    TestTrue(TEXT("Old running atlas still loads"), GyeolPose::Parse(PoseFixture(8, true, 96, 16), Atlas));
    TestFalse(TEXT("Old art does not enable unprovided frames"), Atlas.bAttackTransitions);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackVisualTimingTest, "Gyeol.Art.AttackVisualTiming",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FAttackVisualTimingTest::RunTest(const FString& Parameters)
{
    const double Startup = 5.0 / 60, Active = 4.0 / 60, Recovery = 15.0 / 60;
    const auto Pose = [&](double T, bool bNew = true) { return FString(GyeolPose::AttackPose(T, Startup, Active, Recovery, bNew)); };
    TestEqual(TEXT("Input immediately changes the drawing"), Pose(0), FString(TEXT("attack_ready")));
    TestEqual(TEXT("Anticipation middle"), Pose(Startup * .5), FString(TEXT("windup")));
    TestEqual(TEXT("Extension before hit window"), Pose(Startup - 1.e-6), FString(TEXT("attack_extend")));
    TestEqual(TEXT("Impact starts exactly on hit window"), Pose(Startup), FString(TEXT("impact")));
    TestEqual(TEXT("Impact holds through active time"), Pose(Startup + Active - 1.e-6), FString(TEXT("impact")));
    TestEqual(TEXT("Recovery starts on gameplay boundary"), Pose(Startup + Active), FString(TEXT("recover")));
    TestEqual(TEXT("Recovery middle"), Pose(Startup + Active + Recovery * .5), FString(TEXT("recover_mid")));
    TestEqual(TEXT("Recovery approaches guard"), Pose(Startup + Active + Recovery * .9), FString(TEXT("recover_end")));
    TestEqual(TEXT("Legacy anticipation retained"), Pose(0, false), FString(TEXT("windup")));
    TestEqual(TEXT("Legacy recovery retained"), Pose(.39, false), FString(TEXT("recover")));
    for (const int32 Hz : {30, 60, 120})
    {
        for (int32 Frame = 0; Frame < Hz * .4; ++Frame)
        {
            const double T = Frame / double(Hz);
            const bool bHit = T >= Startup && T < Startup + Active;
            TestEqual(TEXT("Frame rate does not shift visual hit phase"), Pose(T) == TEXT("impact"), bHit);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseDensityTest, "Gyeol.Art.PoseDensity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPoseDensityTest::RunTest(const FString& Parameters)
{
    for (const int32 Cell : {64, 96})
    {
        GyeolPose::FAtlas Atlas;
        if (!TestTrue(TEXT("Density-specific walking atlas parses"), GyeolPose::Parse(PoseFixture(8, true, Cell), Atlas))) { continue; }
        const double UnitsPerPixel = 2080.0 / (Cell * 10);
        TestEqual(TEXT("All 72 cells retained"), Atlas.Frames.Num(), 72);
        TestTrue(TEXT("Cell has the same physical width"), FMath::IsNearlyEqual(Atlas.Cell.X * UnitsPerPixel, 208.0));
        TestTrue(TEXT("Foot X remains fixed in world units"), FMath::IsNearlyEqual(Atlas.Pivot.X * UnitsPerPixel, 104.0));
        TestTrue(TEXT("Foot Y remains fixed in world units"), FMath::IsNearlyEqual(Atlas.Pivot.Y * UnitsPerPixel, 182.0));
        TestEqual(TEXT("Visual timing unchanged"), Atlas.Frames[0].DurationMs, 150);
        TestFalse(TEXT("Out-of-bounds pivot rejected at either density"), GyeolPose::Parse(
            PoseFixture(8, true, Cell).Replace(*FString::Printf(TEXT("\"y\":%d}}]"), Cell * 7 / 8),
                *FString::Printf(TEXT("\"y\":%d}}]"), Cell + 1)), Atlas));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWalkingAtlasTest, "Gyeol.Art.WalkingAtlas",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FWalkingAtlasTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true);
    TestTrue(TEXT("72-cell walking atlas loads"), GyeolPose::Parse(Valid, Atlas));
    TestTrue(TEXT("Walking capability detected"), Atlas.bWalking);
    TestEqual(TEXT("Eight views, not frame count divided by five"), Atlas.ViewCount, 8);
    TestFalse(TEXT("Missing walk pose rejected"), GyeolPose::Parse(Valid.Replace(TEXT("walk3.se"), TEXT("unknown.se")), Atlas));
    TestFalse(TEXT("Failure clears walking flag"), Atlas.bWalking);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStrideClockTest, "Gyeol.Art.StrideClock",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FStrideClockTest::RunTest(const FString& Parameters)
{
    for (const int32 Hz : {30, 60, 120})
    {
        GyeolPose::FStrideClock Gait;
        TestEqual(TEXT("First sample is idle"), Gait.Sample(FVector::ZeroVector, true), INDEX_NONE);
        for (int32 I = 1; I <= Hz; ++I)
        { Gait.Sample(FVector(100.0 * I / Hz, 0, 0), true); }
        TestTrue(TEXT("Distance independent of sample rate"), FMath::IsNearlyEqual(Gait.Distance, 100.0, 1.e-6));
        TestEqual(TEXT("Wall/stop cannot walk in place"), Gait.Sample(FVector(100, 0, 0), true), INDEX_NONE);
        TestEqual(TEXT("Vertical motion is not walking"), Gait.Sample(FVector(100, 0, 10), true), INDEX_NONE);
        TestEqual(TEXT("Turning preserves phase"), Gait.Sample(FVector(100, 10, 10), true), 2);
        TestTrue(TEXT("Turn adds distance only"), FMath::IsNearlyEqual(Gait.Distance, 110.0));
        Gait.Sample(FVector(100, 80, 10), true);
        TestTrue(TEXT("Full cycle wraps"), FMath::IsNearlyZero(Gait.Distance, 1.e-6));
        TestEqual(TEXT("Action overrides movement"), Gait.Sample(FVector(110, 80, 10), false), INDEX_NONE);
        TestEqual(TEXT("Teleport is not a step"), Gait.Sample(FVector(900, 80, 10), true), INDEX_NONE);
        TestEqual(TEXT("Teleport resets phase"), Gait.Distance, 0.0);
        Gait.Reset();
        TestEqual(TEXT("Explicit restart has no movement"), Gait.Sample(FVector(0, 0, 0), true), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEightPoseGaitTest, "Gyeol.Art.EightPoseGait",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FEightPoseGaitTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    TestTrue(TEXT("104-cell atlas parses"), GyeolPose::Parse(PoseFixture(8, true, 96, 8), Atlas));
    TestEqual(TEXT("Eight walk poses per direction"), Atlas.WalkPoseCount, 8);
    TestFalse(TEXT("Missing eighth pose rejected"), GyeolPose::Parse(PoseFixture(8, true, 96, 8).Replace(TEXT("walk7.n"), TEXT("unknown.n")), Atlas));
    double Cycle = 0;
    TestTrue(TEXT("Legacy four-pose default retained"), GyeolPose::ParseLocomotion(TEXT("{}"), 4, Cycle));
    TestEqual(TEXT("Legacy cycle remains 180"), Cycle, 180.0);
    TestFalse(TEXT("New gait requires its calibration"), GyeolPose::ParseLocomotion(TEXT("{}"), 8, Cycle));
    const FString Valid = TEXT(R"({"locomotion":{"pose_count":8,"cycle_cm":128}})");
    TestTrue(TEXT("Calibrated gait loads"), GyeolPose::ParseLocomotion(Valid, 8, Cycle));
    TestEqual(TEXT("Calibrated cycle"), Cycle, 128.0);
    TestFalse(TEXT("Count mismatch rejected"), GyeolPose::ParseLocomotion(Valid, 4, Cycle));
    for (const TCHAR* Bad : {TEXT("0"), TEXT("-1"), TEXT("1001"), TEXT("null"), TEXT("true")})
    { TestFalse(TEXT("Invalid distance rejected"), GyeolPose::ParseLocomotion(Valid.Replace(TEXT("128"), Bad), 8, Cycle)); }
    for (const int32 Hz : {30, 60, 120})
    {
        GyeolPose::FStrideClock Gait;
        Gait.CycleCm = 128;
        Gait.PoseCount = 8;
        Gait.Sample(FVector::ZeroVector, true);
        TSet<int32> Seen;
        for (int32 I = 1; I <= Hz; ++I)
        { Seen.Add(Gait.Sample(FVector(128.0 * I / Hz, 0, 0), true)); }
        TestEqual(TEXT("All eight poses at each tick rate"), Seen.Num(), 8);
        TestTrue(TEXT("128cm closes the cycle"), FMath::IsNearlyZero(Gait.Distance, 1.e-6));
        TestEqual(TEXT("Stop is immediately idle"), Gait.Sample(FVector(128, 0, 0), true), INDEX_NONE);
        TestEqual(TEXT("Action overrides calibrated gait"), Gait.Sample(FVector(130, 0, 0), false), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunningGaitTest, "Gyeol.Art.RunningGait",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FRunningGaitTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8, true, 96, 16);
    TestTrue(TEXT("168-cell running atlas parses"), GyeolPose::Parse(Valid, Atlas));
    TestEqual(TEXT("Sixteen poses per direction"), Atlas.WalkPoseCount, 16);
    TestFalse(TEXT("Missing final run pose rejected"), GyeolPose::Parse(Valid.Replace(TEXT("walk15.n"), TEXT("unknown.n")), Atlas));
    double Cycle = 0;
    const FString Calibration = TEXT(R"({"locomotion":{"pose_count":16,"cycle_cm":240}})");
    TestTrue(TEXT("Running calibration loads"), GyeolPose::ParseLocomotion(Calibration, 16, Cycle));
    TestEqual(TEXT("Running cycle length"), Cycle, 240.0);
    TestFalse(TEXT("Running cannot silently use old cycle"), GyeolPose::ParseLocomotion(TEXT("{}"), 16, Cycle));
    TestFalse(TEXT("Wrong pose count rejected"), GyeolPose::ParseLocomotion(Calibration, 8, Cycle));
    for (const int32 Hz : {30, 60, 120})
    {
        GyeolPose::FStrideClock Gait;
        Gait.CycleCm = 240;
        Gait.PoseCount = 16;
        Gait.Sample(FVector::ZeroVector, true);
        TSet<int32> Seen;
        for (int32 I = 1; I <= Hz; ++I)
        { Seen.Add(Gait.Sample(FVector(500.0 * I / Hz, 0, 0), true)); }
        TestEqual(TEXT("All sixteen poses at normal speed"), Seen.Num(), 16);
        TestTrue(TEXT("500cm has two cycles and 20cm remainder"), FMath::IsNearlyEqual(Gait.Distance, 20.0, 1.e-6));
        TestEqual(TEXT("Stop remains immediate"), Gait.Sample(FVector(500, 0, 0), true), INDEX_NONE);
        TestEqual(TEXT("Attack overrides running"), Gait.Sample(FVector(502, 0, 0), false), INDEX_NONE);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseAtlasDirectionsTest, "Gyeol.Art.PoseAtlasDirections",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPoseAtlasDirectionsTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    const FString Valid = PoseFixture(8);
    TestTrue(TEXT("Forty unmirrored pose cells load"), GyeolPose::Parse(Valid, Atlas));
    TestEqual(TEXT("Eight views"), Atlas.ViewCount, 8);
    TestEqual(TEXT("Every pose in every direction"), Atlas.Frames.Num(), 40);
    if (Atlas.Frames.Num() == 40)
    {
        TestEqual(TEXT("North impact tag"), Atlas.Frames[12].Name, FName(TEXT("impact.n")));
        TestEqual(TEXT("North impact row"), Atlas.Frames[12].Rect.Min, FIntPoint(128, 128));
    }
    TestFalse(TEXT("Missing / duplicate direction rejected"), GyeolPose::Parse(Valid.Replace(TEXT("idle.ne"), TEXT("idle.e")), Atlas));
    TestFalse(TEXT("Unknown direction rejected"), GyeolPose::Parse(Valid.Replace(TEXT("dash.se"), TEXT("dash.other")), Atlas));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScreenDirectionTest, "Gyeol.Art.ScreenDirections",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FScreenDirectionTest::RunTest(const FString& Parameters)
{
    for (const double Pitch : {-30.0, -35.0})
    {
        for (const double Yaw : {0.0, 45.0, 137.0})
        {
            const FRotator View(Pitch, Yaw, 0);
            const FRotationMatrix Basis(View);
            for (int32 Degrees = 0; Degrees < 360; ++Degrees)
            {
                const double Angle = FMath::DegreesToRadians(double(Degrees));
                const FVector2D Desired(FMath::Cos(Angle), FMath::Sin(Angle));
                const FVector Ground = GyeolPose::GroundFromScreen(Desired, View);
                const FVector2D Projection(FVector::DotProduct(Ground, Basis.GetUnitAxis(EAxis::Y)),
                    FVector::DotProduct(Ground, Basis.GetUnitAxis(EAxis::Z)));
                TestTrue(TEXT("Inverse projection preserves screen direction"), Projection.GetSafeNormal().Equals(Desired, 1.e-5));
                TestEqual(TEXT("Nearest screen sector, not world yaw"), GyeolPose::ScreenDirection(Ground, View), ((Degrees + 22) / 45) % 8);
            }
        }
    }
    TestEqual(TEXT("Zero direction keeps previous facing"), GyeolPose::ScreenDirection(FVector::ZeroVector, FRotator(-30, 45, 0), 6), 6);
    TestEqual(TEXT("Height does not change facing"), GyeolPose::ScreenDirection(FVector(0, 0, 100), FRotator(-30, 45, 0), 2), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseAtlasValidTest, "Gyeol.Art.PoseAtlasValid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPoseAtlasValidTest::RunTest(const FString& Parameters)
{
    GyeolPose::FAtlas Atlas;
    TestTrue(TEXT("Fixed-cell atlas loads"), GyeolPose::Parse(PoseFixture(), Atlas));
    TestEqual(TEXT("All five poses retained"), Atlas.Frames.Num(), 5);
    TestEqual(TEXT("Shared foot pivot retained"), Atlas.Pivot, FIntPoint(32, 56));
    if (Atlas.Frames.Num() == 5)
    {
        TestEqual(TEXT("Impact uses its own UV rectangle"), Atlas.Frames[2].Rect.Min.X, 128);
        TestEqual(TEXT("Pose name survives"), Atlas.Frames[2].Name, FName(TEXT("impact")));
    }
    TestTrue(TEXT("Visual duration can change independently"), GyeolPose::Parse(PoseFixture().Replace(TEXT("150"), TEXT("67")), Atlas));
    for (const double Pitch : {-30.0, -35.0})
    {
        const FRotator View(Pitch, 45.0, 0.0);
        const FRotationMatrix Basis(View);
        const FVector Offset = GyeolPose::GroundClearanceOffset(6.0, 3.25, View);
        TestTrue(TEXT("Depth adjustment preserves screen X"), FMath::IsNearlyZero(FVector::DotProduct(Offset, Basis.GetUnitAxis(EAxis::Y)), 1.e-8));
        TestTrue(TEXT("Depth adjustment preserves screen Y"), FMath::IsNearlyZero(FVector::DotProduct(Offset, Basis.GetUnitAxis(EAxis::Z)), 1.e-8));
        TestTrue(TEXT("Lowest opaque pixel clears flat ground"),
            FMath::IsNearlyEqual(Offset.Z - 6.0 * 3.25 * Basis.GetUnitAxis(EAxis::Z).Z, 0.5, 1.e-8));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPoseAtlasRejectTest, "Gyeol.Art.PoseAtlasRejectsInvalid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPoseAtlasRejectTest::RunTest(const FString& Parameters)
{
    const FString Valid = PoseFixture();
    const TArray<FString> Bad = {TEXT("null"), TEXT("{}"),
        Valid.Replace(TEXT("\"to\":0"), TEXT("\"to\":4")),
        Valid.Replace(TEXT("\"trimmed\":false"), TEXT("\"trimmed\":true")),
        Valid.Replace(TEXT("\"x\":64"), TEXT("\"x\":0")),
        Valid.Replace(TEXT("\"pivot\""), TEXT("\"missingPivot\"")),
        Valid.Replace(TEXT("\"x\":32"), TEXT("\"x\":65")),
        Valid.Replace(TEXT("\"duration\":150"), TEXT("\"duration\":150.5")),
        Valid.Replace(TEXT("\"w\":320"), TEXT("\"w\":319"))};
    for (int32 I = 0; I < Bad.Num(); ++I)
    {
        GyeolPose::FAtlas Atlas;
        GyeolPose::Parse(Valid, Atlas);
        TestFalse(FString::Printf(TEXT("Malformed variant %d rejected"), I), GyeolPose::Parse(Bad[I], Atlas));
        TestEqual(TEXT("Failure leaves no stale frames"), Atlas.Frames.Num(), 0);
    }
    return true;
}
#endif
