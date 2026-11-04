#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GyeolPose
{
    struct FFrame { FIntRect Rect; FName Name; int32 DurationMs = 0; };
    struct FNativeStrike
    {
        int32 PoseCount = 0;
        int32 ContactFrame = 0;
        int32 ActiveEndFrame = 0;
        bool IsValid() const { return PoseCount >= 4 && PoseCount <= 240 && ContactFrame > 0
            && ContactFrame < ActiveEndFrame && ActiveEndFrame < PoseCount; }
        int32 Frame(double Elapsed) const
        {
            if (!IsValid() || !FMath::IsFinite(Elapsed) || Elapsed < 0) { return 0; }
            return FMath::FloorToInt(FMath::Clamp(Elapsed * 60.0 + 1.e-7, 0.0, double(PoseCount - 1)));
        }
        FName Pose(double Elapsed) const { return FName(*FString::Printf(TEXT("jab%d"), Frame(Elapsed))); }
        int32 TimedFrame(double Elapsed, double Startup, double Active, double Recovery) const
        {
            if (!IsValid() || !FMath::IsFinite(Elapsed) || Elapsed < 0
                || !FMath::IsFinite(Startup) || !FMath::IsFinite(Active) || !FMath::IsFinite(Recovery)
                || Startup <= 0 || Active <= 0 || Recovery <= 0) { return 0; }
            // Re-time only playback. Source contact/recovery landmarks stay aligned with gameplay.
            const double Sample = Elapsed < Startup ? ContactFrame * (Elapsed / Startup)
                : Elapsed < Startup + Active ? ContactFrame + (ActiveEndFrame - ContactFrame) * ((Elapsed - Startup) / Active)
                : ActiveEndFrame + (PoseCount - ActiveEndFrame) * ((Elapsed - Startup - Active) / Recovery);
            return FMath::FloorToInt(FMath::Clamp(Sample + 1.e-7, 0.0, double(PoseCount - 1)));
        }
        FName TimedPose(double Elapsed, double Startup, double Active, double Recovery) const
        { return FName(*FString::Printf(TEXT("jab%d"), TimedFrame(Elapsed, Startup, Active, Recovery))); }
    };
    // Version 1: authored ticks 0..24 fit the existing action; 24..36 are visual-only.
    struct FDashPlayback
    {
        uint64 Instance = 0;
        int32 Bucket = 0;
        int32 RecoveryTick = 24;
        double EndTime = 0, LastTime = 0;
        bool bSeenDash = false, bRecovering = false;
        void Reset() { *this = {}; }
        int32 ReturnPhase() const { return (Bucket * 6 + 36) % 48; }
        int32 Sample(bool bDash, bool bNeutral, uint64 ActionInstance, double Elapsed,
            double GameTime, double DashSeconds, int32 WalkPhase)
        {
            if (!FMath::IsFinite(GameTime) || GameTime < 0 || !FMath::IsFinite(Elapsed) || Elapsed < 0
                || !FMath::IsFinite(DashSeconds) || DashSeconds <= 0 || (bSeenDash && GameTime < LastTime))
            { Reset(); return INDEX_NONE; }
            LastTime = GameTime;
            if (bDash)
            {
                if (!bSeenDash || Instance != ActionInstance)
                {
                    Bucket = WalkPhase >= 0 && WalkPhase < 48 ? ((WalkPhase + 3) / 6) % 8 : 0;
                    Instance = ActionInstance;
                    EndTime = GameTime - Elapsed + DashSeconds;
                }
                bSeenDash = true;
                bRecovering = false;
                return FMath::FloorToInt(FMath::Clamp(Elapsed / DashSeconds * 24.0 + 1.e-7, 0.0, 24.0));
            }
            if (!bNeutral || !bSeenDash || ActionInstance != Instance || GameTime + 1.e-7 < EndTime
                || GameTime - EndTime >= .2 - 1.e-7)
            { Reset(); return INDEX_NONE; }
            bRecovering = true;
            RecoveryTick = 24 + FMath::FloorToInt(FMath::Clamp((GameTime - EndTime) * 60.0 + 1.e-7, 0.0, 11.0));
            return RecoveryTick;
        }
        FName Pose(int32 Tick) const
        {
            return Tick <= 0 || Tick >= 36
                ? FName(*FString::Printf(TEXT("walk%d"), Tick <= 0 ? Bucket * 6 : ReturnPhase()))
                : FName(*FString::Printf(TEXT("dash_phase%d_%d"), Bucket, Tick));
        }
    };
    struct FAtlas { FIntPoint Size; FIntPoint Cell; FIntPoint Pivot; int32 ViewCount = 1; int32 WalkPoseCount = 0; FNativeStrike NativeStrike; FNativeStrike NativeKick; bool bPhasedDash = false; bool bWalking = false; bool bAttackTransitions = false; bool bMovementEntry = false; bool bLocomotionOnly = false; bool bStopTransitions = false; bool bRepresentativeStrike = false; bool bRepresentativeStrikeEntry = false; TArray<FFrame> Frames; TMap<FName, FVector2D> ContactAnchors; };

    inline int32 RepresentativeStrikeFrame(double Elapsed, double Startup, double Active, double Recovery, bool bImpact)
    {
        if (bImpact) { return 5; }
        if (!FMath::IsFinite(Elapsed) || Elapsed < 0 || Startup <= 0 || Active <= 0 || Recovery <= 0)
        { return 0; }
        const double Frame = Elapsed < Startup ? 5 * Elapsed / Startup
            : Elapsed < Startup + Active ? 5 + 4 * (Elapsed - Startup) / Active
            : 9 + 15 * (Elapsed - Startup - Active) / Recovery;
        return FMath::FloorToInt(FMath::Clamp(Frame + 1.e-7, 0.0, 24.0));
    }

    inline FName RepresentativeStrikePose(double Elapsed, double Startup, double Active, double Recovery, bool bImpact)
    {
        return FName(*FString::Printf(TEXT("jab%d"),
            RepresentativeStrikeFrame(Elapsed, Startup, Active, Recovery, bImpact)));
    }

    inline FName RepresentativeStrikeEntryPose(int32 Frame, int32 Phase,
        int32 StopBucket = INDEX_NONE, int32 StopStep = INDEX_NONE)
    {
        Frame = FMath::Clamp(Frame, 0, 24);
        if (Frame == 0 && StopBucket >= 0 && StopBucket < 8 && StopStep >= 0 && StopStep < 6)
        { return FName(*FString::Printf(TEXT("stop%d_%d"), StopBucket, StopStep)); }
        if (Phase >= 0 && Phase < 48 && Frame < 4)
        {
            return Frame == 0 ? FName(*FString::Printf(TEXT("walk%d"), Phase))
                : FName(*FString::Printf(TEXT("jab_entry%d_%d"), ((Phase + 3) / 6) % 8, Frame));
        }
        return FName(*FString::Printf(TEXT("jab%d"), Frame));
    }

    struct FAttackEntry
    {
        uint64 Instance = 0;
        int32 Phase = INDEX_NONE;
        int32 FacingSector = INDEX_NONE;
        int32 StopBucket = INDEX_NONE;
        int32 StopStep = INDEX_NONE;
        void Observe(bool bAttacking, uint64 ActionInstance, int32 DisplayedWalkPhase,
            int32 PoseCount = 16, int32 DisplayedFacing = INDEX_NONE,
            int32 DisplayedStopBucket = INDEX_NONE, int32 DisplayedStopStep = INDEX_NONE)
        {
            if (!bAttacking) { Instance = 0; Phase = FacingSector = StopBucket = StopStep = INDEX_NONE; }
            else if (Instance != ActionInstance)
            {
                Instance = ActionInstance;
                Phase = DisplayedWalkPhase >= 0 && DisplayedWalkPhase < PoseCount ? DisplayedWalkPhase : INDEX_NONE;
                FacingSector = DisplayedFacing >= 0 && DisplayedFacing < 8 ? DisplayedFacing : INDEX_NONE;
                StopBucket = StopStep = INDEX_NONE;
                if (Phase == INDEX_NONE && PoseCount == 48 && DisplayedStopBucket >= 0 && DisplayedStopBucket < 8
                    && DisplayedStopStep >= 0 && DisplayedStopStep < 6)
                {
                    StopBucket = DisplayedStopBucket;
                    StopStep = DisplayedStopStep;
                    // The current rig's first two settling samples are closer to the run-entry path.
                    // Later samples keep the standing jab; no extra drawing or delayed hit window.
                    if (StopStep < 2) { Phase = StopBucket * 6; }
                }
            }
        }
    };

    // Visual subdivision only: never shifts hit windows, cancels or action completion.
    inline const TCHAR* AttackPose(double Elapsed, double Startup, double Active, double Recovery, bool bTransitions)
    {
        if (Elapsed < Startup)
        {
            if (!bTransitions) { return TEXT("windup"); }
            return Elapsed < Startup / 3.0 ? TEXT("attack_ready")
                : Elapsed < Startup * 2.0 / 3.0 ? TEXT("windup") : TEXT("attack_extend");
        }
        if (Elapsed < Startup + Active) { return TEXT("impact"); }
        if (!bTransitions || Elapsed < Startup + Active + Recovery / 3.0) { return TEXT("recover"); }
        return Elapsed < Startup + Active + Recovery * 2.0 / 3.0 ? TEXT("recover_mid") : TEXT("recover_end");
    }

    inline FName MovementAttackPose(double Elapsed, double Startup, double Active, double Recovery,
        bool bTransitions, bool bMovementEntry, int32 WalkPhase)
    {
        if (bMovementEntry && WalkPhase >= 0 && WalkPhase < 16 && Elapsed < Startup * 2.0 / 3.0)
        {
            return FName(*FString::Printf(TEXT("%s%d"), Elapsed < Startup / 3.0 ? TEXT("attack_entry") : TEXT("attack_settle"), WalkPhase));
        }
        return FName(AttackPose(Elapsed, Startup, Active, Recovery, bTransitions));
    }

    inline bool ParseLocomotion(const FString& Json, int32 PoseCount, double& CycleCm, bool bRequire60Hz = false)
    {
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) { return false; }
        CycleCm = 180.0;
        if (!Root->HasField(TEXT("locomotion"))) { return PoseCount == 4 && !bRequire60Hz; }
        const TSharedPtr<FJsonObject>* Gait;
        double Count, Cycle;
        if (!Root->TryGetObjectField(TEXT("locomotion"), Gait)
            || !(*Gait)->HasTypedField<EJson::Number>(TEXT("pose_count"))
            || !(*Gait)->HasTypedField<EJson::Number>(TEXT("cycle_cm"))
            || !(*Gait)->TryGetNumberField(TEXT("pose_count"), Count) || Count != PoseCount
            || !(*Gait)->TryGetNumberField(TEXT("cycle_cm"), Cycle) || !FMath::IsFinite(Cycle)
            || Cycle <= 0 || Cycle > 1000) { return false; }
        double Fps;
        if (bRequire60Hz && (!(*Gait)->HasTypedField<EJson::Number>(TEXT("fps"))
            || !(*Gait)->TryGetNumberField(TEXT("fps"), Fps) || Fps != 60)) { return false; }
        CycleCm = Cycle;
        return true;
    }

    inline const TCHAR* DirectionName(int32 Index)
    {
        static const TCHAR* Names[] = {TEXT("e"), TEXT("ne"), TEXT("n"), TEXT("nw"), TEXT("w"), TEXT("sw"), TEXT("s"), TEXT("se")};
        return Names[FMath::Clamp(Index, 0, 7)];
    }

    inline FName PoseKey(FName Pose, int32 Direction, int32 ViewCount)
    {
        return ViewCount == 1 ? Pose : FName(*(Pose.ToString() + TEXT(".") + DirectionName(Direction)));
    }

    inline int32 ScreenDirection(const FVector& GroundDirection, const FRotator& View, int32 Previous = 0)
    {
        const FRotationMatrix Basis(View);
        const FVector Ground(GroundDirection.X, GroundDirection.Y, 0.0);
        const double X = FVector::DotProduct(Ground, Basis.GetUnitAxis(EAxis::Y));
        const double Y = FVector::DotProduct(Ground, Basis.GetUnitAxis(EAxis::Z));
        if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X * X + Y * Y < 1.e-10) { return FMath::Clamp(Previous, 0, 7); }
        const int32 Sector = FMath::FloorToInt(FMath::Atan2(Y, X) / (UE_PI / 4.0) + 0.5);
        return (Sector + 8) % 8;
    }

    inline FVector GroundFromScreen(const FVector2D& Screen, const FRotator& View)
    {
        const FRotationMatrix Basis(View);
        const FVector Up = Basis.GetUnitAxis(EAxis::Z);
        const FVector UpGround(Up.X, Up.Y, 0.0);
        if (UpGround.SizeSquared() < 1.e-8) { return FVector::ZeroVector; }
        return (Basis.GetUnitAxis(EAxis::Y) * Screen.X + UpGround * (Screen.Y / UpGround.SizeSquared())).GetSafeNormal2D();
    }

    inline FVector StrikeEntryDirection(int32 Frame, int32 SourceSector, const FVector& Target, const FRotator& View)
    {
        if (Frame < 0 || Frame >= 4 || SourceSector < 0 || SourceSector >= 8) { return Target; }
        const int32 TargetSector = ScreenDirection(Target, View, SourceSector);
        // Shortest screen arc; an exact half-turn consistently takes the negative arc.
        const int32 Delta = (TargetSector - SourceSector + 12) % 8 - 4;
        const double T = Frame / 4.0, Weight = T * T * (3 - 2 * T);
        // Quantize before projecting, so half-sector ties cannot flip due to
        // atan2 roundoff in the later sprite selection.
        const int32 Sector = (FMath::FloorToInt(SourceSector + Delta * Weight + .5) + 8) % 8;
        const double Angle = Sector * UE_PI / 4;
        return GroundFromScreen(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)), View);
    }

    inline FVector GroundClearanceOffset(double PixelsBelowPivot, double UnitsPerPixel, const FRotator& View)
    {
        const FRotationMatrix Basis(View);
        const FVector Forward = Basis.GetUnitAxis(EAxis::X);
        if (PixelsBelowPivot <= 0.0 || UnitsPerPixel <= 0.0 || FMath::Abs(Forward.Z) < 0.01) { return FVector::ZeroVector; }
        const double UpZ = Basis.GetUnitAxis(EAxis::Z).Z;
        return Forward * ((PixelsBelowPivot * UnitsPerPixel * UpZ + 0.5) / Forward.Z);
    }

    inline FVector ContactOffset(const FVector2D& Pixel, const FIntPoint& Pivot, double UnitsPerPixel, const FRotator& View)
    {
        const FRotationMatrix Basis(View);
        return (Basis.GetUnitAxis(EAxis::Y) * (Pixel.X - Pivot.X)
            + Basis.GetUnitAxis(EAxis::Z) * (Pivot.Y - Pixel.Y)) * UnitsPerPixel;
    }

    inline FVector ContactOnFrontPlane(const FVector& Contact, const FVector& Front, const FVector& ViewForward)
    {
        return Contact + ViewForward * FVector::DotProduct(Front - Contact, ViewForward);
    }

    inline bool Integer(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32& Out)
    {
        double N;
        if (!O || !O->TryGetNumberField(Key, N) || !FMath::IsFinite(N) || N < 0 || N > 16384 || N != FMath::FloorToDouble(N)) { return false; }
        Out = static_cast<int32>(N);
        return true;
    }

    // Fixed untrimmed cells, one frame per named pose; either one view or eight screen sectors.
    inline bool Parse(const FString& Json, FAtlas& Out)
    {
        Out = {};
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) { return false; }
        const TSharedPtr<FJsonObject> *Meta, *Size;
        const TArray<TSharedPtr<FJsonValue>> *Frames, *Tags, *Slices;
        if (!Root->TryGetObjectField(TEXT("meta"), Meta) || !(*Meta)->TryGetObjectField(TEXT("size"), Size)
            || !Root->TryGetArrayField(TEXT("frames"), Frames)
            || !(*Meta)->TryGetArrayField(TEXT("frameTags"), Tags) || Tags->Num() != Frames->Num()
            || !(*Meta)->TryGetArrayField(TEXT("slices"), Slices) || Slices->Num() != 1) { return false; }
        FAtlas Parsed;
        const TCHAR* DashFields[] = {TEXT("gyeol_dash_version"), TEXT("gyeol_dash_fps"),
            TEXT("gyeol_dash_entry_end_frame"), TEXT("gyeol_dash_hold_end_frame"),
            TEXT("gyeol_dash_end_frame"), TEXT("gyeol_dash_bucket_count")};
        const int32 DashValues[] = {1, 60, 12, 24, 36, 8};
        bool bAnyDashField = false;
        for (const TCHAR* Field : DashFields) { bAnyDashField |= (*Meta)->HasField(Field); }
        if (bAnyDashField)
        {
            int32 Count;
            if (!(*Meta)->HasField(TEXT("gyeol_representative_strike_version"))
                || !Integer(*Meta, TEXT("gyeol_walk_pose_count"), Count) || Count != 48) { return false; }
            for (int32 I = 0; I < 6; ++I)
            {
                int32 Value;
                if (!(*Meta)->HasTypedField<EJson::Number>(DashFields[I])
                    || !Integer(*Meta, DashFields[I], Value) || Value != DashValues[I]) { return false; }
            }
            Parsed.bPhasedDash = true;
        }
        const int32 DashFrames = Parsed.bPhasedDash ? 8 * 35 * 8 : 0;
        if ((*Meta)->HasField(TEXT("gyeol_representative_strike_entry_version")))
        {
            int32 Version;
            if (!(*Meta)->HasField(TEXT("gyeol_representative_strike_version"))
                || !Integer(*Meta, TEXT("gyeol_representative_strike_entry_version"), Version)
                || Version != 1) { return false; }
            Parsed.bRepresentativeStrikeEntry = true;
        }
        if ((*Meta)->HasField(TEXT("gyeol_representative_strike_version")))
        {
            int32 Version, Count;
            if (!Integer(*Meta, TEXT("gyeol_representative_strike_version"), Version)
                || !Integer(*Meta, TEXT("gyeol_walk_pose_count"), Count)
                || (*Meta)->HasField(TEXT("gyeol_locomotion_only"))) { return false; }
            if (Version == 1)
            {
                if (Count != 48 || Frames->Num() != (Parsed.bRepresentativeStrikeEntry ? 1176 : 984) + DashFrames
                    || !(*Meta)->HasField(TEXT("gyeol_stop_transition_version"))) { return false; }
            }
            else if (Version == 2)
            {
                int32 Fps;
                if (Count < 4 || Count > 120 || Parsed.bRepresentativeStrikeEntry
                    || (*Meta)->HasField(TEXT("gyeol_stop_transition_version"))
                    || !Integer(*Meta, TEXT("gyeol_strike_fps"), Fps) || Fps != 60
                    || !Integer(*Meta, TEXT("gyeol_strike_pose_count"), Parsed.NativeStrike.PoseCount)
                    || !Integer(*Meta, TEXT("gyeol_strike_contact_frame"), Parsed.NativeStrike.ContactFrame)
                    || !Integer(*Meta, TEXT("gyeol_strike_active_end_frame"), Parsed.NativeStrike.ActiveEndFrame)
                    || !Parsed.NativeStrike.IsValid()) { return false; }
                if ((*Meta)->HasField(TEXT("gyeol_kick_pose_count")))
                {
                    if (!Integer(*Meta, TEXT("gyeol_kick_pose_count"), Parsed.NativeKick.PoseCount)
                        || !Integer(*Meta, TEXT("gyeol_kick_contact_frame"), Parsed.NativeKick.ContactFrame)
                        || !Integer(*Meta, TEXT("gyeol_kick_active_end_frame"), Parsed.NativeKick.ActiveEndFrame)
                        || !Parsed.NativeKick.IsValid()) { return false; }
                }
                if (Frames->Num() != (Count + Parsed.NativeStrike.PoseCount + Parsed.NativeKick.PoseCount + 2) * 8 + DashFrames) { return false; }
                Parsed.WalkPoseCount = Count;
            }
            else { return false; }
            Parsed.bRepresentativeStrike = true;
        }
        if (((*Meta)->HasField(TEXT("gyeol_kick_pose_count"))
            || (*Meta)->HasField(TEXT("gyeol_kick_contact_frame"))
            || (*Meta)->HasField(TEXT("gyeol_kick_active_end_frame"))) && !Parsed.NativeKick.IsValid()) { return false; }
        if ((*Meta)->HasField(TEXT("gyeol_stop_transition_version")))
        {
            int32 Version;
            if ((!Parsed.bRepresentativeStrike && !(*Meta)->HasField(TEXT("gyeol_locomotion_only")))
                || !Integer(*Meta, TEXT("gyeol_stop_transition_version"), Version) || Version != 1)
            { return false; }
            Parsed.bStopTransitions = true;
        }
        if ((*Meta)->HasField(TEXT("gyeol_locomotion_only")))
        {
            int32 Count;
            if (!(*Meta)->TryGetBoolField(TEXT("gyeol_locomotion_only"), Parsed.bLocomotionOnly)
                || !Parsed.bLocomotionOnly || !Integer(*Meta, TEXT("gyeol_walk_pose_count"), Count)
                || Count < 4 || Count > 120 || (Parsed.bStopTransitions && Count != 48)
                || Frames->Num() != (Count + 1 + (Parsed.bStopTransitions ? 48 : 0)) * 8) { return false; }
            Parsed.WalkPoseCount = Count;
        }
        else if (!Parsed.bRepresentativeStrike && Frames->Num() != 5 && Frames->Num() != 40 && Frames->Num() != 72 &&
            Frames->Num() != 104 && Frames->Num() != 168 && Frames->Num() != 200 && Frames->Num() != 456)
        { return false; }
        Parsed.ViewCount = Frames->Num() == 5 ? 1 : 8;
        Parsed.bAttackTransitions = !Parsed.bRepresentativeStrike && !Parsed.bLocomotionOnly && Frames->Num() >= 200;
        Parsed.bMovementEntry = !Parsed.bLocomotionOnly && !Parsed.bRepresentativeStrike && Frames->Num() == 456;
        if (!Parsed.bLocomotionOnly && !Parsed.NativeStrike.IsValid())
        { Parsed.WalkPoseCount = Parsed.bRepresentativeStrike ? 48 : Frames->Num() >= 168 ? 16 : Frames->Num() == 104 ? 8 : Frames->Num() == 72 ? 4 : 0; }
        Parsed.bWalking = Parsed.WalkPoseCount > 0;
        if (!Integer(*Size, TEXT("w"), Parsed.Size.X) || !Integer(*Size, TEXT("h"), Parsed.Size.Y)
            || Parsed.Size.X <= 0 || Parsed.Size.Y <= 0) { return false; }
        for (const auto& Value : *Frames)
        {
            const TSharedPtr<FJsonObject>* Frame;
            const TSharedPtr<FJsonObject> *Rect, *Source, *Offset;
            bool Rotated = true, Trimmed = true;
            if (!Value->TryGetObject(Frame) || !(*Frame)->TryGetObjectField(TEXT("frame"), Rect)
                || !(*Frame)->TryGetObjectField(TEXT("sourceSize"), Source)
                || !(*Frame)->TryGetObjectField(TEXT("spriteSourceSize"), Offset)
                || !(*Frame)->TryGetBoolField(TEXT("rotated"), Rotated) || Rotated
                || !(*Frame)->TryGetBoolField(TEXT("trimmed"), Trimmed) || Trimmed) { return false; }
            int32 X, Y, W, H, SW, SH, OX, OY, OW, OH, Duration;
            if (!Integer(*Rect, TEXT("x"), X) || !Integer(*Rect, TEXT("y"), Y)
                || !Integer(*Rect, TEXT("w"), W) || !Integer(*Rect, TEXT("h"), H)
                || !Integer(*Source, TEXT("w"), SW) || !Integer(*Source, TEXT("h"), SH)
                || !Integer(*Offset, TEXT("x"), OX) || !Integer(*Offset, TEXT("y"), OY)
                || !Integer(*Offset, TEXT("w"), OW) || !Integer(*Offset, TEXT("h"), OH)
                || !Integer(*Frame, TEXT("duration"), Duration) || Duration == 0
                || W <= 0 || H <= 0 || W != SW || H != SH || OX != 0 || OY != 0 || OW != W || OH != H
                || X + W > Parsed.Size.X || Y + H > Parsed.Size.Y) { return false; }
            if (Parsed.Frames.Num() && Parsed.Cell != FIntPoint(W, H)) { return false; }
            const FIntRect NewRect(X, Y, X + W, Y + H);
            for (const auto& Existing : Parsed.Frames)
            {
                if (NewRect.Min.X < Existing.Rect.Max.X && NewRect.Max.X > Existing.Rect.Min.X
                    && NewRect.Min.Y < Existing.Rect.Max.Y && NewRect.Max.Y > Existing.Rect.Min.Y) { return false; }
            }
            Parsed.Cell = FIntPoint(W, H);
            Parsed.Frames.Add({NewRect, NAME_None, Duration});
        }
        TSet<FName> Names;
        for (const auto& Value : *Tags)
        {
            const TSharedPtr<FJsonObject>* Tag;
            FString Name;
            int32 From, To;
            if (!Value->TryGetObject(Tag) || !(*Tag)->TryGetStringField(TEXT("name"), Name)
                || !Integer(*Tag, TEXT("from"), From) || !Integer(*Tag, TEXT("to"), To)
                || From != To || !Parsed.Frames.IsValidIndex(From) || !Parsed.Frames[From].Name.IsNone()
                || Names.Contains(FName(*Name))) { return false; }
            Names.Add(FName(*Name));
            Parsed.Frames[From].Name = FName(*Name);
        }
        for (const TCHAR* Name : {TEXT("idle"), TEXT("windup"), TEXT("impact"), TEXT("recover"), TEXT("dash")})
        {
            if (Parsed.bLocomotionOnly && FCString::Strcmp(Name, TEXT("idle")) != 0) { continue; }
            if (Parsed.bRepresentativeStrike && FCString::Strcmp(Name, TEXT("idle")) != 0
                && FCString::Strcmp(Name, TEXT("dash")) != 0) { continue; }
            for (int32 Direction = 0; Direction < Parsed.ViewCount; ++Direction)
            {
                if (!Names.Contains(PoseKey(FName(Name), Direction, Parsed.ViewCount))) { return false; }
            }
        }
        if (Parsed.bPhasedDash)
        {
            for (int32 Bucket = 0; Bucket < 8; ++Bucket)
            {
                for (int32 Tick = 1; Tick < 36; ++Tick)
                {
                    for (int32 Direction = 0; Direction < 8; ++Direction)
                    {
                        if (!Names.Contains(PoseKey(FName(*FString::Printf(
                            TEXT("dash_phase%d_%d"), Bucket, Tick)), Direction, 8))) { return false; }
                    }
                }
            }
        }
        if (Parsed.bRepresentativeStrike)
        {
            const int32 StrikeCount = Parsed.NativeStrike.IsValid() ? Parsed.NativeStrike.PoseCount : 25;
            for (int32 I = 0; I < StrikeCount; ++I)
            {
                for (int32 Direction = 0; Direction < 8; ++Direction)
                {
                    if (!Names.Contains(PoseKey(FName(*FString::Printf(TEXT("jab%d"), I)), Direction, 8))) { return false; }
                }
            }
        }
        for (int32 I = 0; I < Parsed.NativeKick.PoseCount; ++I)
        {
            for (int32 Direction = 0; Direction < 8; ++Direction)
            {
                if (!Names.Contains(PoseKey(FName(*FString::Printf(TEXT("kick%d"), I)), Direction, 8))) { return false; }
            }
        }
        if (Parsed.bRepresentativeStrikeEntry)
        {
            for (int32 Bucket = 0; Bucket < 8; ++Bucket)
            {
                for (int32 Step = 1; Step <= 3; ++Step)
                {
                    for (int32 Direction = 0; Direction < 8; ++Direction)
                    {
                        if (!Names.Contains(PoseKey(FName(*FString::Printf(
                            TEXT("jab_entry%d_%d"), Bucket, Step)), Direction, 8))) { return false; }
                    }
                }
            }
        }
        if (Parsed.bWalking)
        {
            for (int32 Phase = 0; Phase < Parsed.WalkPoseCount; ++Phase)
            {
                const FName Name(*FString::Printf(TEXT("walk%d"), Phase));
                for (int32 Direction = 0; Direction < 8; ++Direction)
                { if (!Names.Contains(PoseKey(Name, Direction, 8))) { return false; } }
            }
        }
        if (Parsed.bAttackTransitions)
        {
            for (const TCHAR* Name : {TEXT("attack_ready"), TEXT("attack_extend"), TEXT("recover_mid"), TEXT("recover_end")})
            {
                for (int32 Direction = 0; Direction < 8; ++Direction)
                { if (!Names.Contains(PoseKey(FName(Name), Direction, 8))) { return false; } }
            }
        }
        if (Parsed.bStopTransitions)
        {
            for (int32 Bucket = 0; Bucket < 8; ++Bucket)
            {
                for (int32 Step = 0; Step < 6; ++Step)
                {
                    const FName Name(*FString::Printf(TEXT("stop%d_%d"), Bucket, Step));
                    for (int32 Direction = 0; Direction < 8; ++Direction)
                    { if (!Names.Contains(PoseKey(Name, Direction, 8))) { return false; } }
                }
            }
        }
        if (Parsed.bMovementEntry)
        {
            for (int32 Phase = 0; Phase < 16; ++Phase)
            {
                for (const TCHAR* Stage : {TEXT("attack_entry"), TEXT("attack_settle")})
                {
                    const FName Name(*FString::Printf(TEXT("%s%d"), Stage, Phase));
                    for (int32 Direction = 0; Direction < 8; ++Direction)
                    { if (!Names.Contains(PoseKey(Name, Direction, 8))) { return false; } }
                }
            }
        }
        const TSharedPtr<FJsonObject> *Slice, *Key, *Pivot, *Bounds;
        const TArray<TSharedPtr<FJsonValue>>* Keys;
        FString SliceName;
        int32 Frame, X, Y, W, H;
        if (!(*Slices)[0]->TryGetObject(Slice) || !(*Slice)->TryGetStringField(TEXT("name"), SliceName) || SliceName != TEXT("foot")
            || !(*Slice)->TryGetArrayField(TEXT("keys"), Keys) || Keys->Num() != 1 || !(*Keys)[0]->TryGetObject(Key)
            || !Integer(*Key, TEXT("frame"), Frame) || Frame != 0
            || !(*Key)->TryGetObjectField(TEXT("pivot"), Pivot) || !(*Key)->TryGetObjectField(TEXT("bounds"), Bounds)
            || !Integer(*Pivot, TEXT("x"), Parsed.Pivot.X) || !Integer(*Pivot, TEXT("y"), Parsed.Pivot.Y)
            || !Integer(*Bounds, TEXT("x"), X) || !Integer(*Bounds, TEXT("y"), Y)
            || !Integer(*Bounds, TEXT("w"), W) || !Integer(*Bounds, TEXT("h"), H)
            || X != 0 || Y != 0 || FIntPoint(W, H) != Parsed.Cell
            || Parsed.Pivot.X > W || Parsed.Pivot.Y > H) { return false; }
        if ((*Meta)->HasField(TEXT("gyeol_contact_anchors")))
        {
            const TSharedPtr<FJsonObject>* Anchors;
            if (!(*Meta)->TryGetObjectField(TEXT("gyeol_contact_anchors"), Anchors)
                || (*Anchors)->Values.IsEmpty()) { return false; }
            for (const auto& Pair : (*Anchors)->Values)
            {
                const TSharedPtr<FJsonObject>* Point;
                double PX, PY;
                if (!Names.Contains(FName(*Pair.Key)) || !Pair.Value->TryGetObject(Point)
                    || !(*Point)->TryGetNumberField(TEXT("x"), PX) || !(*Point)->TryGetNumberField(TEXT("y"), PY)
                    || !FMath::IsFinite(PX) || !FMath::IsFinite(PY)
                    || PX < 0 || PY < 0 || PX >= W || PY >= H) { return false; }
                Parsed.ContactAnchors.Add(FName(*Pair.Key), FVector2D(PX, PY));
            }
        }
        Out = MoveTemp(Parsed);
        return true;
    }
}
