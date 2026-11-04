// SPDX-License-Identifier: Apache-2.0
using UnrealBuildTool;

public class HanjiSampleTarget : TargetRules
{
	public HanjiSampleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HanjiSample");
	}
}
