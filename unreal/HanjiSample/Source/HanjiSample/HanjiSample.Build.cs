// SPDX-License-Identifier: Apache-2.0
using UnrealBuildTool;

public class HanjiSample : ModuleRules
{
	public HanjiSample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Hanji" });
	}
}
