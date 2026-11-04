// SPDX-License-Identifier: Apache-2.0
#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "HanjiAssetLibrary.h"
#include "HanjiCamera.h"
#include "HanjiClipTable.h"
#include "HanjiSpriteComponent.h"
#include "HanjiSubsystem.h"
#include "HanjiTestListener.h"
#include "Misc/AutomationTest.h"

namespace HanjiTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/** A 4x2 grid of 32x48 cells: idle (2 frames) and jab (6 frames), both tagged without a heading, so the
	 *  same frames serve all eight headings. */
	FString AtlasJson()
	{
		FString Frames;
		const int32 Durations[8] = {200, 200, 50, 50, 40, 40, 100, 150};
		for (int32 I = 0; I < 8; ++I)
		{
			Frames += FString::Printf(TEXT("%s{\"filename\":\"f%d\",\"frame\":{\"x\":%d,\"y\":%d,\"w\":32,\"h\":48},\"sourceSize\":{\"w\":32,\"h\":48},\"duration\":%d}"),
				I == 0 ? TEXT("") : TEXT(","), I, (I % 4) * 32, (I / 4) * 48, Durations[I]);
		}
		return FString::Printf(TEXT(R"({"frames":[%s],"meta":{"image":"atlas.png","size":{"w":128,"h":96},
			"frameTags":[{"name":"idle","from":0,"to":1},{"name":"jab","from":2,"to":7}],
			"slices":[{"name":"foot","keys":[{"frame":0,"bounds":{"x":0,"y":0,"w":32,"h":48},"pivot":{"x":12,"y":44}}]}]}})"), *Frames);
	}

	const TCHAR* Overlay = TEXT(R"({"format":"hanji-clips","version":1,"actions":[
		{"name":"jab","loop":false,"contact_frame":2,"active_end_frame":4,
		 "events":{"0":["whoosh"],"2":["hit"]},
		 "hitboxes":[{"name":"fist","kind":"attack","frames":[2,3],"box":[24,10,6,6],"heading":"e"}]}]})");

	const TCHAR* Policies = TEXT(R"({"format":"hanji-policies","version":1,"states":[
		{"state":"idle"},
		{"state":"jab","lock_facing":true,"timing":{"startup_s":0.2,"active_s":0.1,"recovery_s":0.3},
		 "hit_stop":{"duration_s":0.07,"time_scale":0,"who":"victim"}}]})");

	UHanjiClipTable* MakeTable()
	{
		UHanjiClipTable* Table = NewObject<UHanjiClipTable>(GetTransientPackage());
		Table->SourceJson = AtlasJson();
		Table->OverlayJson = Overlay;
		Table->PoliciesJson = Policies;
		Table->Rebuild();
		return Table;
	}

	struct FScene
	{
		UWorld* World = nullptr;
		AActor* Actor = nullptr;
		UHanjiSpriteComponent* Sprite = nullptr;
		UHanjiTestListener* Listener = nullptr;

		explicit FScene(UHanjiClipTable* Table, float CentimetresPerPixel = 2.0f, float Tilt = 0.0f)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HanjiTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			Actor = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Actor->SetActorLocation(FVector(100.0, 50.0, 10.0));
			Sprite = NewObject<UHanjiSpriteComponent>(Actor, TEXT("Sprite"));
			Sprite->ClipTable = Table;
			Sprite->CentimetresPerPixel = CentimetresPerPixel;
			Sprite->TiltDegrees = Tilt;
			Sprite->bSnapToPixelGrid = false;
			Sprite->SetupAttachment(Root);
			Sprite->RegisterComponent();
			Listener = NewObject<UHanjiTestListener>();
			Listener->AddToRoot();
			Sprite->OnFrameEvent.AddDynamic(Listener, &UHanjiTestListener::OnEvent);
			Sprite->OnActionFinished.AddDynamic(Listener, &UHanjiTestListener::OnFinished);
		}

		~FScene()
		{
			Listener->RemoveFromRoot();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiClipTableTest, "Hanji.ClipTable.BuildsFromJson", HanjiTests::Flags)
bool FHanjiClipTableTest::RunTest(const FString&)
{
	UHanjiClipTable* Table = HanjiTests::MakeTable();
	TestTrue(TEXT("The table builds"), Table->IsUsable());
	TestEqual(TEXT("No errors"), Table->Errors.Num(), 0);
	TestEqual(TEXT("Two actions"), Table->GetActionNames().Num(), 2);

	Table->OverlayJson = TEXT(R"({"format":"hanji-clips","version":1,"actions":[{"name":"kick","contact_frame":1}]})");
	TestFalse(TEXT("An overlay naming a missing action fails the build"), Table->Rebuild());
	TestTrue(TEXT("and says why"), Table->Errors.Num() > 0 && Table->Errors[0].Contains(TEXT("kick")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiQuadMeshTest, "Hanji.Assets.QuadMesh", HanjiTests::Flags)
bool FHanjiQuadMeshTest::RunTest(const FString&)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, UHanjiAssetLibrary::QuadMeshPath);
	if (!TestNotNull(TEXT("The plugin ships its quad mesh"), Mesh)) return false;
	const FBox Box = Mesh->GetBoundingBox();
	TestTrue(TEXT("One unit wide around the pivot"), FMath::IsNearlyEqual(Box.Min.Y, -0.5, 1e-4) && FMath::IsNearlyEqual(Box.Max.Y, 0.5, 1e-4));
	TestTrue(TEXT("One unit high from the pivot up"), FMath::IsNearlyEqual(Box.Min.Z, 0.0, 1e-4) && FMath::IsNearlyEqual(Box.Max.Z, 1.0, 1e-4));
	TestTrue(TEXT("Flat in X"), FMath::IsNearlyEqual(Box.Min.X, 0.0, 1e-4) && FMath::IsNearlyEqual(Box.Max.X, 0.0, 1e-4));
	TestNotNull(TEXT("The plugin ships its lit sprite material"), LoadObject<UMaterialInterface>(nullptr, UHanjiAssetLibrary::LitMaterialPath));
	TestNotNull(TEXT("The plugin ships its unlit sprite material"), LoadObject<UMaterialInterface>(nullptr, UHanjiAssetLibrary::UnlitMaterialPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiPlacementTest, "Hanji.Sprite.PivotStandsOnTheAnchor", HanjiTests::Flags)
bool FHanjiPlacementTest::RunTest(const FString&)
{
	for (const float Tilt : {0.0f, 15.0f, 30.0f})
	{
		HanjiTests::FScene Scene(HanjiTests::MakeTable(), 2.0f, Tilt);
		const FRotator View(-30.0, 45.0, 0.0);
		Scene.Sprite->HanjiAdvance(0.0f, View);
		const FVector Anchor = Scene.Actor->GetActorLocation();
		const FVector Pivot = Scene.Sprite->GetPixelWorldLocation(FVector2D(12.0, 44.0));
		TestTrue(FString::Printf(TEXT("Pivot pixel on the anchor at tilt %.0f"), Tilt), Pivot.Equals(Anchor, 1e-3));

		// The cell keeps its screen size: 32 px wide, 48 px high on screen at 2 cm per pixel.
		const FRotationMatrix Basis(View);
		const FVector Top = Scene.Sprite->GetPixelWorldLocation(FVector2D(12.0, 0.0));
		const FVector Left = Scene.Sprite->GetPixelWorldLocation(FVector2D(0.0, 44.0));
		const FVector Right = Scene.Sprite->GetPixelWorldLocation(FVector2D(32.0, 44.0));
		TestTrue(TEXT("Screen height of 44 px"), FMath::IsNearlyEqual(FVector::DotProduct(Top - Pivot, Basis.GetUnitAxis(EAxis::Z)), 88.0, 1e-3));
		TestTrue(TEXT("Screen width of 32 px"), FMath::IsNearlyEqual(FVector::DotProduct(Right - Left, Basis.GetUnitAxis(EAxis::Y)), 64.0, 1e-3));
		if (Tilt == 0.0f) TestTrue(TEXT("Upright quads stand vertical"), FMath::IsNearlyEqual((Top - Pivot).GetSafeNormal().Z, 1.0, 1e-6));

		// Moving the actor moves the sprite; the pivot follows exactly.
		Scene.Actor->SetActorLocation(Anchor + FVector(37.0, -11.0, 0.0));
		Scene.Sprite->HanjiAdvance(0.0f, View);
		TestTrue(TEXT("Pivot follows the actor"), Scene.Sprite->GetPixelWorldLocation(FVector2D(12.0, 44.0)).Equals(Scene.Actor->GetActorLocation(), 1e-3));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiFrameTest, "Hanji.Sprite.FramesEventsAndUV", HanjiTests::Flags)
bool FHanjiFrameTest::RunTest(const FString&)
{
	HanjiTests::FScene Scene(HanjiTests::MakeTable());
	const FRotator View(-30.0, 45.0, 0.0);
	TestEqual(TEXT("Starts in its initial state"), Scene.Sprite->GetCurrentAction(), FName(TEXT("idle")));
	Scene.Sprite->HanjiAdvance(0.25f, View);
	TestEqual(TEXT("Idle frame 1 after 250 ms"), Scene.Sprite->GetFrame(), 1);
	const FVector4 UV = Scene.Sprite->GetFrameUV();
	TestTrue(TEXT("UV of cell 1"), FMath::IsNearlyEqual(UV.X, 0.25, 1e-6) && FMath::IsNearlyEqual(UV.Y, 0.0, 1e-6)
		&& FMath::IsNearlyEqual(UV.Z, 0.25, 1e-6) && FMath::IsNearlyEqual(UV.W, 0.5, 1e-6));

	// The jab is timed by its state's policy: the hit lands on the first 60 Hz tick at or after 200 ms.
	TestTrue(TEXT("Enters the jab state"), Scene.Sprite->PlayState(TEXT("jab")));
	TestEqual(TEXT("Whoosh fires as the jab starts"), Scene.Listener->Events.Num() > 0 ? Scene.Listener->Events.Last() : FString(), FString(TEXT("whoosh@0")));
	double Elapsed = 0.0;
	while (!Scene.Listener->Events.Contains(TEXT("hit@2")) && Elapsed < 1.0)
	{
		Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
		Elapsed += 1.0 / 60.0;
	}
	TestTrue(TEXT("Hit fires"), Scene.Listener->Events.Contains(TEXT("hit@2")));
	TestTrue(TEXT("on the first tick at or after the startup"), Elapsed >= 0.2 - 1e-6 && Elapsed < 0.2 + 1.0 / 60.0);
	TestTrue(TEXT("The active window is open"), Scene.Sprite->IsInActiveWindow());

	// Hitboxes are live on the contact frame in heading e only.
	Scene.Sprite->SetFacingDirection(FRotationMatrix(View).GetUnitAxis(EAxis::Y));  // screen right = heading e
	Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
	TestEqual(TEXT("Facing is locked during the jab"), Scene.Sprite->GetHeading(), FName(TEXT("s")));
	Scene.Sprite->PlayState(TEXT("idle"));
	Scene.Sprite->HanjiAdvance(0.0f, View);
	TestEqual(TEXT("Facing follows once the lock ends"), Scene.Sprite->GetHeading(), FName(TEXT("e")));
	Scene.Sprite->PlayState(TEXT("jab"));
	for (int32 I = 0; I < 13; ++I) Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
	const TArray<FHanjiBox> Boxes = Scene.Sprite->GetActiveBoxes();
	TestEqual(TEXT("One attack box on the contact frame"), Boxes.Num(), 1);
	if (Boxes.Num() == 1)
	{
		TestEqual(TEXT("It is the fist"), Boxes[0].Name, FName(TEXT("fist")));
		TestTrue(TEXT("Centred where the pixels are"), Boxes[0].Center.Equals(Scene.Sprite->GetPixelWorldLocation(FVector2D(27.0, 13.0)), 1e-3));
	}

	// Finishing raises the event once.
	for (int32 I = 0; I < 60; ++I) Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
	TestEqual(TEXT("Finished once"), Scene.Listener->Finished.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiHitStopTest, "Hanji.Sprite.HitStopFreezesAndRestores", HanjiTests::Flags)
bool FHanjiHitStopTest::RunTest(const FString&)
{
	HanjiTests::FScene Scene(HanjiTests::MakeTable());
	const FRotator View(-30.0, 45.0, 0.0);
	Scene.Sprite->PlayState(TEXT("jab"));
	for (int32 I = 0; I < 13; ++I) Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
	const int32 Frame = Scene.Sprite->GetFrame();
	const int32 EventsBefore = Scene.Listener->Events.Num();
	TestTrue(TEXT("The state's hit-stop applies"), Scene.Sprite->ApplyStateHitStop());
	Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
	TestTrue(TEXT("Stopped"), Scene.Sprite->IsHitStopped());
	TestTrue(TEXT("The owner follows the stop"), Scene.Actor->CustomTimeDilation < 0.01f);
	for (int32 I = 0; I < 3; ++I) Scene.Sprite->HanjiAdvance(1.0f / 60.0f, View);
	TestEqual(TEXT("The frame holds"), Scene.Sprite->GetFrame(), Frame);
	TestEqual(TEXT("No event fires again"), Scene.Listener->Events.Num(), EventsBefore);
	Scene.Sprite->HanjiAdvance(0.05f, View);
	TestFalse(TEXT("The stop ends on real time"), Scene.Sprite->IsHitStopped());
	TestEqual(TEXT("The owner is restored"), Scene.Actor->CustomTimeDilation, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiSubsystemTest, "Hanji.Subsystem.AdvancesEverySprite", HanjiTests::Flags)
bool FHanjiSubsystemTest::RunTest(const FString&)
{
	HanjiTests::FScene Scene(HanjiTests::MakeTable());
	UHanjiSubsystem* Subsystem = Scene.World->GetSubsystem<UHanjiSubsystem>();
	if (!TestNotNull(TEXT("The world has the subsystem"), Subsystem)) return false;
	TestEqual(TEXT("The sprite registered itself"), Subsystem->NumSprites(), 1);
	TestFalse(TEXT("Sprites do not tick themselves"), Scene.Sprite->PrimaryComponentTick.bCanEverTick);
	Subsystem->SetViewRotation(FRotator(-30.0, 45.0, 0.0));
	Subsystem->Tick(0.25f);
	TestEqual(TEXT("The subsystem advanced the sprite"), Scene.Sprite->GetFrame(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiShadowAttachTest, "Hanji.Sprite.ShadowAndPixelAttachments", HanjiTests::Flags)
bool FHanjiShadowAttachTest::RunTest(const FString&)
{
	HanjiTests::FScene Scene(HanjiTests::MakeTable());
	const FRotator View(-30.0, 45.0, 0.0);
	Scene.Sprite->HanjiAdvance(0.0f, View);
	UDecalComponent* Shadow = Scene.Sprite->GetBlobShadow();
	if (!TestNotNull(TEXT("A blob shadow is made"), Shadow)) return false;
	TestTrue(TEXT("Under the pivot"), Shadow->GetComponentLocation().Equals(Scene.Actor->GetActorLocation(), 1e-3));
	TestTrue(TEXT("Projecting down"), Shadow->GetForwardVector().Equals(FVector(0, 0, -1), 1e-4));
	TestNotNull(TEXT("With the plugin's decal material"), Shadow->GetDecalMaterial());
	TestFalse(TEXT("The sprite ignores decals"), Scene.Sprite->bReceivesDecals);

	USceneComponent* Spark = NewObject<USceneComponent>(Scene.Actor);
	Spark->SetupAttachment(Scene.Actor->GetRootComponent());
	Spark->RegisterComponent();
	Scene.Sprite->AttachToPixel(Spark, FVector2D(27.0, 13.0), 0.0f);
	TestTrue(TEXT("The effect sits on its pixel"), Spark->GetComponentLocation().Equals(Scene.Sprite->GetPixelWorldLocation(FVector2D(27.0, 13.0)), 1e-3));
	Scene.Sprite->HanjiAdvance(0.0f, View);
	Scene.Actor->SetActorLocation(Scene.Actor->GetActorLocation() + FVector(40.0, 0.0, 0.0));
	Scene.Sprite->HanjiAdvance(0.0f, View);
	TestTrue(TEXT("and follows the sprite"), Spark->GetComponentLocation().Equals(Scene.Sprite->GetPixelWorldLocation(FVector2D(27.0, 13.0)), 1e-3));
	Scene.Sprite->AttachToPixel(Spark, FVector2D(27.0, 13.0), 5.0f);
	const FVector Bias = Spark->GetComponentLocation() - Scene.Sprite->GetPixelWorldLocation(FVector2D(27.0, 13.0));
	TestTrue(TEXT("Depth bias moves it toward the camera"), Bias.Equals(-View.Vector() * 5.0, 1e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHanjiCameraTest, "Hanji.Camera.ZoomStepsAndPixelSnap", HanjiTests::Flags)
bool FHanjiCameraTest::RunTest(const FString&)
{
	HanjiTests::FScene Scene(HanjiTests::MakeTable());
	AHanjiCamera* Camera = Scene.World->SpawnActor<AHanjiCamera>();
	Camera->SetFollowTarget(Scene.Actor);
	Camera->SnapToTarget();
	TestTrue(TEXT("Starts at 100 %"), FMath::IsNearlyEqual(Camera->GetZoom(), 1.0f));
	Camera->SetZoomStep(2);
	for (int32 I = 0; I < 60; ++I) Camera->Step(1.0f / 60.0f);
	TestTrue(TEXT("Settles exactly at 110 %"), Camera->GetZoom() == 1.1f || FMath::IsNearlyEqual(Camera->GetZoom(), 1.1f, 1e-6f));
	TestTrue(TEXT("A screen pixel grows with the view"), FMath::IsNearlyEqual(Camera->GetWorldPerPixel(), 1850.0f * 1.1f / 640.0f, 1e-3f));

	// The rendered camera position lies on the screen-pixel lattice while the focus stays continuous.
	Scene.Actor->SetActorLocation(FVector(123.4, -56.7, 10.0));
	for (int32 I = 0; I < 10; ++I) Camera->Step(1.0f / 60.0f);
	const FRotationMatrix Basis(Camera->ViewRotation);
	const FVector Shown = Camera->GetActorLocation() + Camera->ViewRotation.Vector() * Camera->Distance;
	for (const EAxis::Type Axis : {EAxis::Y, EAxis::Z})
	{
		const double Pixel = FVector::DotProduct(Shown, Basis.GetUnitAxis(Axis)) / Camera->GetWorldPerPixel();
		TestTrue(TEXT("Camera on the pixel lattice"), FMath::IsNearlyEqual(Pixel, FMath::RoundToDouble(Pixel), 1e-4));
	}
	TestTrue(TEXT("Orthographic width follows the zoom"), FMath::IsNearlyEqual(Camera->GetCameraComponent()->OrthoWidth, 1850.0f * 1.1f, 1e-2f));
	Camera->CycleZoom();
	TestEqual(TEXT("Zoom steps wrap"), Camera->GetZoomStep(), 0);
	return true;
}

#endif
