// Copyright 2026 MANIFOLD. All Rights Reserved.

using UnrealBuildTool;

public class MANIFOLDGameplay : ModuleRules
{
    public MANIFOLDGameplay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Gameplay orchestrates every runtime system, so it depends on all of them.
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AudioMixer",
            "MANIFOLDCore",
            "MANIFOLDKernelsOrbits",
            "MANIFOLDKernelsFluids",
            "MANIFOLDKernelsHarmonics",
            "MANIFOLDKernelsWaves",
            "MANIFOLDKernelsRhythm",
            "MANIFOLDKernelsGears",
            "MANIFOLDKernelsCircuits",
            "MANIFOLDCorrespond",
            "MANIFOLDTelemetry"
        });

        CppStandard = CppStandardVersion.Cpp20;
    }
}
