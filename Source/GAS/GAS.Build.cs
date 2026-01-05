// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GAS : ModuleRules
{
	public GAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UE 5.3+ 에서 상대 경로 include를 허용 (레거시 호환성)
		// 예: #include "Sound/GS_AudioManager.h" 형태의 상대 경로 지원
		bLegacyParentIncludePaths = true;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			//"GameLiftSDK",
			"SlateCore",
			"Slate",
			"UMG",
            "CoreOnline",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineSubsystemSteam",
			"AkAudio",
			"Wwise",
			"WwiseSoundEngine",
            "Niagara",
            "PhysicsCore",
            "CommonUI",
            "CommonInput",
            "Chooser",
            "PoseSearch",
            "GeometryCollectionEngine",
            "NavigationSystem",
            "MediaAssets",
            "SignificanceManager",
            "GeometryCache"
        });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Linux) || (Target.Platform == UnrealTargetPlatform.Mac))
        {
            // 아래 모듈들이 필요합니다.
            PublicDependencyModuleNames.AddRange(new string[] { "SteamShared", "Steamworks", "OnlineSubsystemSteam" });

            // Steamworks 라이브러리를 링크합니다.
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
