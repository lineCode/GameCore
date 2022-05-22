// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BFL_HitResultHelpers.generated.h"



/**
 * A collection of helpful functions related to Hit Results.
 * Helpful for getting certain data from Hit Results and more.
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_HitResultHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/** Returns true if the two given hits were from the same trace */
	UFUNCTION(BlueprintPure, Category = "HitResultHelpers")
		static bool AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB);
	
	/**
	 * Avoids having to run a square root distance calculation to calculate a hit's trace's length.
	 * If your hit has a known Time value of 0, you minus well just call FVector::Distance() since that is this function's fallback calculation for this case.
	 */
	UFUNCTION(BlueprintPure, Category = "HitResultHelpers")
		static float CheapCalculateTraceLength(const FHitResult& InHit);

};
