// Copyright 2026 MANIFOLD. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TelemetrySystem.generated.h"

/**
 * Single telemetry log event
 */
USTRUCT(BlueprintType)
struct MANIFOLDTELEMETRY_API FTelemetryEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 StepCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Parameters;
};

/**
 * Telemetry logging and Insight-Rate calculation manager (WP S8)
 */
UCLASS(BlueprintType)
class MANIFOLDTELEMETRY_API UTelemetrySystem : public UObject
{
    GENERATED_BODY()

public:
    UTelemetrySystem();

    /** Initialize logging with output filename */
    void InitializeTelemetry(const FString& InLogFileName);

    /** Shutdown and flush logs */
    void ShutdownTelemetry();

    /** Log an event with parameters */
    void LogEvent(const FString& EventType, int64 CurrentStep, float CurrentTime, const TMap<FString, FString>& Parameters);

    /** 
     * Calculate Insight-Rate (discoveries / hour or unit time) 
     * Insight Rate = Total Discovery Events / Simulation Time
     */
    float CalculateInsightRate() const;

    /** Get logged events */
    const TArray<FTelemetryEvent>& GetLoggedEvents() const { return LoggedEvents; }

protected:
    FString LogFilePath;
    TArray<FTelemetryEvent> LoggedEvents;
    
    // Internal file writer
    IFileHandle* FileHandle;
};
