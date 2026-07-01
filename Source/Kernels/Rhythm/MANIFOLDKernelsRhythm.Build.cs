// Copyright 2026 MANIFOLD. All Rights Reserved.

using UnrealBuildTool;

public class MANIFOLDKernelsRhythm : ModuleRules
{
    public MANIFOLDKernelsRhythm(ReadOnlyTargetRules Target) : base(Target)
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
