// SPDX-License-Identifier: Apache-2.0
using UnrealBuildTool;

public class HanjiSampleEditorTarget : TargetRules
{
	public HanjiSampleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HanjiSample");
	}
}
