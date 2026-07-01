using UnrealBuildTool;

public class MANIFOLDKernelsFluids : ModuleRules
{
	public MANIFOLDKernelsFluids(ReadOnlyTargetRules Target) : base(Target)
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
