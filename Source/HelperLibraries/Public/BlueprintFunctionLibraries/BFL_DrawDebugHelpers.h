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
	static void DrawDebugCollisionShape(const FCollisionShape& CollisionShape, const UWorld* InWorld, const FVector& Center, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f, const FQuat& Rotation = FQuat::Identity);
};
