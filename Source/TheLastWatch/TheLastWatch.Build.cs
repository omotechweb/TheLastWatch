// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheLastWatch : ModuleRules
{
	public TheLastWatch(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TheLastWatch",
			"TheLastWatch/Game",
			"TheLastWatch/Variant_Horror",
			"TheLastWatch/Variant_Horror/UI",
			"TheLastWatch/Variant_Shooter",
			"TheLastWatch/Variant_Shooter/AI",
			"TheLastWatch/Variant_Shooter/UI",
			"TheLastWatch/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
