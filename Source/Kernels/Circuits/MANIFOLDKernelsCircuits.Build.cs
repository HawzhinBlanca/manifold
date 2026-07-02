// Copyright 2026 MANIFOLD. All Rights Reserved.

using UnrealBuildTool;

public class MANIFOLDKernelsCircuits : ModuleRules
{
    public MANIFOLDKernelsCircuits(ReadOnlyTargetRules Target) : base(Target)
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
