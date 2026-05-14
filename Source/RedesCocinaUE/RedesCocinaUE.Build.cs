// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RedesCocinaUE : ModuleRules
{
	public RedesCocinaUE(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
    		"PhotonFusion"  
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"RedesCocinaUE",
			"RedesCocinaUE/Variant_Platforming",
			"RedesCocinaUE/Variant_Platforming/Animation",
			"RedesCocinaUE/Variant_Combat",
			"RedesCocinaUE/Variant_Combat/AI",
			"RedesCocinaUE/Variant_Combat/Animation",
			"RedesCocinaUE/Variant_Combat/Gameplay",
			"RedesCocinaUE/Variant_Combat/Interfaces",
			"RedesCocinaUE/Variant_Combat/UI",
			"RedesCocinaUE/Variant_SideScrolling",
			"RedesCocinaUE/Variant_SideScrolling/AI",
			"RedesCocinaUE/Variant_SideScrolling/Gameplay",
			"RedesCocinaUE/Variant_SideScrolling/Interfaces",
			"RedesCocinaUE/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
