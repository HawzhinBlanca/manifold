// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "TelemetrySystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTelemetryInsightRateEventsTest, "MANIFOLD.Telemetry.InsightRateEvents", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FTelemetryInsightRateEventsTest::RunTest(const FString& Parameters)
{
    UTelemetrySystem* Telemetry = NewObject<UTelemetrySystem>();
    FString TestLogName = TEXT("AutomationTestTelemetry.log");
    
    // Initialize
    Telemetry->InitializeTelemetry(TestLogName);
    
    // Log discovery events
    TMap<FString, FString> Params;
    Params.Add(TEXT("Key"), TEXT("Value"));
    
    // Log 3 events spanning up to 10 seconds
    Telemetry->LogEvent(TEXT("Discovery"), 1, 0.0f, Params);
    Telemetry->LogEvent(TEXT("CorrespondenceIgnited"), 10, 5.0f, Params);
    Telemetry->LogEvent(TEXT("Discovery"), 20, 10.0f, Params);
    
    // Shutdown to flush file writer
    Telemetry->ShutdownTelemetry();
    
    // Validate Insight Rate calculation
    // 3 events / 10.0 seconds = 0.3 events per second
    float Rate = Telemetry->CalculateInsightRate();
    UTEST_NEARLY_EQUAL("Insight Rate Calculation (3 events / 10s)", Rate, 0.3f, 1e-4f);
    
    // Verify file output exists on disk
    FString Dir = FPaths::ProjectLogDir();
    FString TestLogPath = FPaths::Combine(Dir, TestLogName);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    UTEST_TRUE("Telemetry log file created", PlatformFile.FileExists(*TestLogPath));

    // Clean up test file
    PlatformFile.DeleteFile(*TestLogPath);

    return true;
}
