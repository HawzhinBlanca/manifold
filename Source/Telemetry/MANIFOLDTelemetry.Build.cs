using UnrealBuildTool;

public class MANIFOLDTelemetry : ModuleRules
{
	public MANIFOLDTelemetry(ReadOnlyTargetRules Target) : base(Target)
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
