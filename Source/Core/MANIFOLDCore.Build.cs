// Copyright 2026 MANIFOLD. All Rights Reserved.

using UnrealBuildTool;

public class MANIFOLDCore : ModuleRules
{
    public MANIFOLDCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Projects"
        });

        // Determinism: disable optimizations that could affect floating-point reproducibility
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            bUseUnity = false;  // More predictable incremental builds
        }

        // C++20 for std::format, concepts, etc.
        CppStandard = CppStandardVersion.Cpp20;
    }
}