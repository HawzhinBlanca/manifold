using UnrealBuildTool;
using System.Collections.Generic;

public class MANIFOLDEditorTarget : TargetRules
{
	public MANIFOLDEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { 
			"MANIFOLDCore", 
			"MANIFOLDKernelsOrbits", 
			"MANIFOLDKernelsFluids",
			"MANIFOLDKernelsHarmonics",
			"MANIFOLDCorrespond",
			"MANIFOLDTelemetry",
			"MANIFOLDGameplay"
		});
	}
}
