// Copyright 2026 MANIFOLD. All Rights Reserved.

using UnrealBuildTool;

public class MANIFOLDKernelsWaves : ModuleRules
{
    public MANIFOLDKernelsWaves(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "MANIFOLDCore"
        });

        CppStandard = CppStandardVersion.Cpp20;
    }
}
