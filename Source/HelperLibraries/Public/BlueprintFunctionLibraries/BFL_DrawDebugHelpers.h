// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "BFL_DrawDebugHelpers.generated.h"



/**
 * 
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_DrawDebugHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/** Accepts a generic FCollisionShape to draw */
	static void DrawDebugCollisionShape(const UWorld* InWorld, const FVector& Center, const FCollisionShape& CollisionShape, const FQuat& Rotation, const FColor& Color, const int32 Segments = 16, const bool bPersistentLines = false, const float LifeTime = -1.f, const uint8 DepthPriority = 0, const float Thickness = 0.f);
};
