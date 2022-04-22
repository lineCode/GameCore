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

};
