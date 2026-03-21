// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Agent : ModuleRules
{
	public Agent(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"Niagara",
			"GeometryCore",
			"GeometryCollectionEngine",
			"GeometryFramework",
			"GeometryScriptingCore",
			"DynamicMesh",
			"ProceduralMeshComponent",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AnimGraph",
				"BlueprintGraph",
				"ControlRig",
				"ControlRigDeveloper",
				"UnrealEd"
			});
		}

		PublicIncludePaths.AddRange(new string[] {
			"Agent",
			"Agent/Backpack",
			"Agent/Backpack/Actors",
			"Agent/Backpack/Components",
			"Agent/Dirty",
			"Agent/Factory",
			"Agent/Material",
			"Agent/Machine",
			"Agent/Objects",
			"Agent/Objects/Actors",
			"Agent/Objects/Assets",
			"Agent/Objects/Components",
			"Agent/Objects/Types",
			"Agent/Tools",
			"Agent/WorldUI",
			"Agent/Vehicle",
			"Agent/Vehicle/Actors",
			"Agent/Vehicle/Components",
			"Agent/Vehicle/Interfaces",
			"Agent/Variant_Platforming",
			"Agent/Variant_Platforming/Animation",
			"Agent/Variant_Combat",
			"Agent/Variant_Combat/AI",
			"Agent/Variant_Combat/Animation",
			"Agent/Variant_Combat/Gameplay",
			"Agent/Variant_Combat/Interfaces",
			"Agent/Variant_Combat/UI",
			"Agent/Variant_SideScrolling",
			"Agent/Variant_SideScrolling/AI",
			"Agent/Variant_SideScrolling/Gameplay",
			"Agent/Variant_SideScrolling/Interfaces",
			"Agent/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
