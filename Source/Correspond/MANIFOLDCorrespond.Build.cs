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

		// JSON parsing for data-driven correspondence content (Build Plan D1).
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json"
		});

		CppStandard = CppStandardVersion.Cpp20;
	}
}
