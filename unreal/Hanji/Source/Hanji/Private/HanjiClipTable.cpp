// SPDX-License-Identifier: Apache-2.0
#include "HanjiClipTable.h"

#include "HanjiCoreIncludes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UHanjiClipTable::UHanjiClipTable() = default;
UHanjiClipTable::~UHanjiClipTable() = default;

namespace
{
	EHanjiSourceFormat Detect(const FString& Json)
	{
		if (Json.Contains(TEXT("\"pose-atlas-contract\""))) return EHanjiSourceFormat::PoseContract;
		if (Json.Contains(TEXT("\"hanji-clips\""))) return EHanjiSourceFormat::HanjiClips;
		return EHanjiSourceFormat::AsepriteJson;
	}

	void Collect(const hanji::Diagnostics& D, TArray<FString>& Errors, TArray<FString>& Warnings)
	{
		for (const std::string& E : D.errors) Errors.Add(HanjiConvert::String(E));
		for (const std::string& W : D.warnings) Warnings.Add(HanjiConvert::String(W));
	}
}

bool UHanjiClipTable::Rebuild()
{
	Table.Reset();
	Policies.Reset();
	Errors.Reset();
	Warnings.Reset();
	if (SourceJson.IsEmpty())
	{
		Errors.Add(TEXT("No source JSON."));
		return false;
	}

	const std::string Source = HanjiConvert::Std(SourceJson);
	const EHanjiSourceFormat Kind = Format == EHanjiSourceFormat::Auto ? Detect(SourceJson) : Format;
	hanji::LoadResult Loaded;
	switch (Kind)
	{
	case EHanjiSourceFormat::PoseContract: Loaded = hanji::load_pose_contract(Source); break;
	case EHanjiSourceFormat::HanjiClips: Loaded = hanji::load_hanji_clips(Source); break;
	default: Loaded = hanji::load_aseprite_json(Source); break;
	}
	Collect(Loaded.diagnostics, Errors, Warnings);
	if (!Loaded.table || !Loaded.diagnostics.ok()) return false;

	hanji::ClipTable Built = std::move(*Loaded.table);
	if (!OverlayJson.IsEmpty())
	{
		const hanji::Diagnostics D = hanji::apply_hanji_overlay(Built, HanjiConvert::Std(OverlayJson));
		Collect(D, Errors, Warnings);
		if (!D.ok()) return false;
	}
	if (!PoliciesJson.IsEmpty())
	{
		hanji::PolicyLoad P = hanji::load_policies(HanjiConvert::Std(PoliciesJson), &Built);
		Collect(P.diagnostics, Errors, Warnings);
		if (!P.table || !P.diagnostics.ok()) return false;
		Policies = MakeShared<hanji::PolicyTable>(std::move(*P.table));
	}
	if (Atlas && (Atlas->GetSizeX() != Built.image_w || Atlas->GetSizeY() != Built.image_h) && Atlas->GetSizeX() > 0)
	{
		Warnings.Add(FString::Printf(TEXT("The atlas texture is %dx%d but the metadata describes %dx%d; frames are placed by the metadata."),
			Atlas->GetSizeX(), Atlas->GetSizeY(), Built.image_w, Built.image_h));
	}
	Table = MakeShared<hanji::ClipTable>(std::move(Built));
	return true;
}

TArray<FName> UHanjiClipTable::GetActionNames() const
{
	TArray<FName> Names;
	if (Table)
	{
		for (const hanji::Action& A : Table->actions) Names.Add(FName(HanjiConvert::String(A.name)));
	}
	return Names;
}

float UHanjiClipTable::GetCentimetresPerPixel() const
{
	return Table ? static_cast<float>(Table->metres_per_pixel * 100.0) : 0.0f;
}

void UHanjiClipTable::PostLoad()
{
	Super::PostLoad();
	Rebuild();
}

#if WITH_EDITOR
void UHanjiClipTable::ReloadFromFiles()
{
	auto Read = [this](const FFilePath& Path, FString& Into)
	{
		if (Path.FilePath.IsEmpty()) return;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *Path.FilePath)) Into = MoveTemp(Text);
		else Errors.Add(FString::Printf(TEXT("Cannot read %s"), *Path.FilePath));
	};
	Read(SourceFile, SourceJson);
	Read(OverlayFile, OverlayJson);
	Read(PoliciesFile, PoliciesJson);
	Rebuild();
	Modify();
}

void UHanjiClipTable::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	Rebuild();
}
#endif
