// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BFL_MathHelpers.generated.h"



/**
 * A collection of common math helpers
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_MathHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	 * Given a segment and a point, does that point lie on the segment?
	 * NOTE: Does 3 distance calculations (3 sqrts)
	 */
	UFUNCTION(BlueprintPure, Category = "MathHelpers|VectorMath")
		static bool PointLiesOnSegment(const FVector& Start, const FVector& End, const FVector& Point);

	/**
	 * Gets the direction from the Location to the point that AimDir is looking at.
	 * E.g. having a weapon's muzzle aim towards the Player's look point.
	 */
	static FVector GetLocationAimDirection(const UWorld* InWorld, const FCollisionQueryParams& Params, const FVector& AimPoint, const FVector& AimDir, const float& MaxRange, const FVector& Location);
};
