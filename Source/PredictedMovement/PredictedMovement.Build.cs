// Copyright (c) 2023 Jared Taylor

using UnrealBuildTool;

public class PredictedMovement : ModuleRules
{
	public PredictedMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"NetCore",
			}
			);
	}
}
