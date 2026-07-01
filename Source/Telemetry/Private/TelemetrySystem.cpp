// Copyright 2026 MANIFOLD. All Rights Reserved.

#include "TelemetrySystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

UTelemetrySystem::UTelemetrySystem()
{
    FileHandle = nullptr;
}

void UTelemetrySystem::BeginDestroy()
{
    // Idempotent with explicit ShutdownTelemetry: closes+null-guards the file handle so
    // a dropped session (e.g. on restart) never leaks it or leaves the log file locked.
    ShutdownTelemetry();
    Super::BeginDestroy();
}

void UTelemetrySystem::InitializeTelemetry(const FString& InLogFileName)
{
    LoggedEvents.Empty();

    FString Dir = FPaths::ProjectLogDir();
    LogFilePath = FPaths::Combine(Dir, InLogFileName);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FileHandle = PlatformFile.OpenWrite(*LogFilePath);
}

void UTelemetrySystem::ShutdownTelemetry()
{
    if (FileHandle)
    {
        FileHandle->Flush();
        delete FileHandle;
        FileHandle = nullptr;
    }
}

void UTelemetrySystem::LogEvent(const FString& EventType, int64 CurrentStep, float CurrentTime, const TMap<FString, FString>& Parameters)
{
    FTelemetryEvent Evt;
    Evt.EventType = EventType;
    Evt.Timestamp = CurrentTime;
    Evt.StepCount = CurrentStep;
    Evt.Parameters = Parameters;

    LoggedEvents.Add(Evt);

    if (FileHandle)
    {
        // Serialize to CSV line
        FString Line = FString::Printf(TEXT("[%f][Step:%lld] Event: %s"), CurrentTime, CurrentStep, *EventType);
        for (const auto& Pair : Parameters)
        {
            Line += FString::Printf(TEXT(", %s: %s"), *Pair.Key, *Pair.Value);
        }
        Line += TEXT("\n");

        FTCHARToUTF8 Converter(*Line);
        FileHandle->Write(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
        FileHandle->Flush();
    }
}

float UTelemetrySystem::CalculateInsightRate() const
{
    if (LoggedEvents.Num() == 0) return 0.0f;

    // Count discovery/correspondence events
    int32 DiscoveryCount = 0;
    float MaxTime = 0.0f;

    for (const FTelemetryEvent& Evt : LoggedEvents)
    {
        if (Evt.EventType == TEXT("CorrespondenceIgnited") || Evt.EventType == TEXT("Discovery"))
        {
            DiscoveryCount++;
        }
        if (Evt.Timestamp > MaxTime)
        {
            MaxTime = Evt.Timestamp;
        }
    }

    if (MaxTime <= 0.0f) return 0.0f;
    return static_cast<float>(DiscoveryCount) / MaxTime; // events per second
}
