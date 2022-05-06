// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class HelperLibraries : ModuleRules
{
	public HelperLibraries(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Private/HelperLibrariesPCH.h";

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine"
			}
		);
	}
}
