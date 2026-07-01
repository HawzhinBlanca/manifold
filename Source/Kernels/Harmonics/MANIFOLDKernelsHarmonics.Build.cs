// Copyright 2026 MANIFOLD. All Rights Reserved.

using UnrealBuildTool;

public class MANIFOLDKernelsHarmonics : ModuleRules
{
    public MANIFOLDKernelsHarmonics(ReadOnlyTargetRules Target) : base(Target)
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
