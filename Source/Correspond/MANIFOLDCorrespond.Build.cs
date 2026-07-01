using UnrealBuildTool;

public class MANIFOLDCorrespond : ModuleRules
{
	public MANIFOLDCorrespond(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"MANIFOLDCore",
			"MANIFOLDKernelsOrbits",
			"MANIFOLDKernelsFluids"
		});
		
		CppStandard = CppStandardVersion.Cpp20;
	}
}
