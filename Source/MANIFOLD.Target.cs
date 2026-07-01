using UnrealBuildTool;
using System.Collections.Generic;

public class MANIFOLDTarget : TargetRules
{
	public MANIFOLDTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { 
			"MANIFOLDCore", 
			"MANIFOLDKernelsOrbits", 
			"MANIFOLDKernelsFluids",
			"MANIFOLDKernelsHarmonics",
			"MANIFOLDKernelsWaves",
			"MANIFOLDCorrespond",
			"MANIFOLDTelemetry",
			"MANIFOLDGameplay"
		});
	}
}
