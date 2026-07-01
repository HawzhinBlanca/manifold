// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Modules/ModuleManager.h"

// MANIFOLDTelemetry has no custom startup/shutdown logic, so the default
// module implementation is sufficient. This IMPLEMENT_MODULE is REQUIRED —
// without it the DLL loads but the engine reports "could not be successfully
// initialized after it was loaded" and aborts startup.
IMPLEMENT_MODULE(FDefaultModuleImpl, MANIFOLDTelemetry)
