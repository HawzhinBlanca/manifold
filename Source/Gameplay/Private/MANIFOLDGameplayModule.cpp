// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Modules/ModuleManager.h"

// MANIFOLDGameplay is the project's PRIMARY game module: it provides the game-module
// boilerplate (GInternalProjectName, allocator wrappers, etc.) that a standalone
// packaged executable links against. The editor exe supplies these itself, which is
// why editor/test builds link without this — a packaged game does not.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MANIFOLDGameplay, "MANIFOLD")
