// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MetaHillSystem : ModuleRules
{
	public MetaHillSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		bEnableExceptions = true;
		
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"yasio"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"yasio"
			});
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
