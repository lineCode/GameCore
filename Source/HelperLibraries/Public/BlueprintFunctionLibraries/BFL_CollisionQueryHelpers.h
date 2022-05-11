// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BFL_CollisionQueryHelpers.generated.h"



/**
 * Extension of FHitResult indicating whether it's an entrance or an exit
 */
USTRUCT()
struct HELPERLIBRARIES_API FExitAwareHitResult : public FHitResult
{
	GENERATED_BODY()

	FExitAwareHitResult()
		: FHitResult()
		, bIsExitHit(false)
	{
	}
	FExitAwareHitResult(const FHitResult& HitResult)
		: FHitResult(HitResult)
		, bIsExitHit(false)
	{
	}

	uint8 bIsExitHit : 1;
};

/**
 * A collection of our custom collision queries
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_CollisionQueryHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static const float SceneCastStartWallAvoidancePadding;


	/**
	 *  Line trace multi that penetrates everything except for what the caller says in ShouldNotPenetrate() TFunction
	 *
	 *  The engine's way of handling collisions is good, but is limiting in some situations. We want to run a trace where
	 *  blocking hits don't actually block the trace, creating a sort of "penetration" feature. The reason we can't just use
	 *  a collision trace channel who'se default response is overlap is because doing so removes the distinction between overlaps
	 *  and blocking hits (e.g. no way to know what is a triggerbox and what is a physical wall). This line trace is effectively
	 *  a LineTraceMultiByChannel() that doesn't get stopped by blocking hits and instead gets stopped by whatever condition the
	 *  caller desires while still preserving the hits' collision responses.
	 *  @param  InWorld                   Static functions require a world to perform anything in
	 *  @param  OutHits                   Array of hits (overlap and blocking) that were found until ShouldStopAtHit condition is met
	 *  @param  InTraceStart              Start location of the ray
	 *  @param  InTraceEnd                End location of the ray
	 *  @param  InTraceChannel            The 'channel' that this ray is in, used to determine which components to hit. Unlike the default UWorld traces, blocking hits for this channel don't stop the trace, but is instead only used for preserving the CollisionResponse of each hit.
	 *  @param  InCollisionQueryParams    Additional parameters used for the trace
	 *  @param  IsHitImpenetrable         TFunction where caller indicates whether provided HitResult should stop the trace
	 *  @return TRUE if hit and stopped at an impenetrable hit.
	 */
	static bool PenetrationLineTrace(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);

	/**
	 * Refer to LineTraceMultiByChannelWithPenetrations() for documentation
	 */
	static bool PenetrationSweep(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);


	/**
	 *  Line trace multi with penetrations that outputs entrance and exit hits in order of the forward tracing direction
	 *  
	 *  @param  OutHits                              Array of entrance and exit hits (overlap and blocking) that were found until ShouldStopAtHit condition is met
	 *  @param  bOptimizeBackwardsSceneCastLength    Only use this if you're not starting the trace inside of geometry, otherwise the exits of any gemometry you're starting inside of may not be found. However, you could possibly still get away with this optimization if you are doing a very lengthy trace, because you are more likely to hit an entrance past the exit of the geometry that you started in. If true, will minimize the backwards tracing distance to go no further than the exit of the furthest entrance.
	 *  @return TRUE if hit and stopped at an impenetrable hit.
	 */
	static bool PenetrationLineTraceWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr, const bool bOptimizeBackwardsSceneCastLength = false);

	/**
	 * Refer to PenetrationLineTraceWithExitHits() for documentation
	 */
	static bool PenetrationSweepWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr, const bool bOptimizeBackwardsSceneCastLength = false);

	/**
	 * Refer to PenetrationLineTraceWithExitHits() for documentation
	 */
	static bool LineTraceMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const bool bOptimizeBackwardsSceneCastLength = false);

	/**
	 * Refer to LineTraceMultiWithExitHits() for documentation
	 */
	static bool SweepMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const bool bOptimizeBackwardsSceneCastLength = false);


private:
	/**
	 * Modifies existing HitResults to respond appropriately to the caller's ECollisionChannel and FCollisionQueryParams.
	 * Outputs modified hits and potentially removes some.
	 * 
	 * @param  InOutHits					Hits to modify
	 * @param  InTraceChannel				The collision channel in which the hits will conform to (e.g. setting FHitResult::bBlockingHit to true because of the hit component's response to our trace channel)
	 * @param  InCollisionQueryParams		The collision query params in which the hits will conform to (e.g. removing blocking hits because of FCollisionQueryParams::bIgnoreBlocks)
	 */
	static void ChangeHitsResponseData(TArray<FHitResult>& InOutHits, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam);




	/**
	 * 
	 */
	static FVector DetermineBackwardsSceneCastStart(const TArray<FHitResult>& InForwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const bool bStoppedByHit, const bool bOptimizeBackwardsSceneCastLength = false);

	/**
	 * 
	 */
	static void MakeBackwardsHitsDataRelativeToForwadsSceneCast(TArray<FHitResult>& InOutBackwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const FVector& InBackwardsStart, const bool bStoppedByHit, const bool bOptimizeBackwardsSceneCastLength = false);

	/**
	 * 
	 */
	static void OrderHitResultsInForwardsDirection(TArray<FExitAwareHitResult>& OutOrderedHitResults, const TArray<FHitResult>& InEntranceHitResults, const TArray<FHitResult>& InExitHitResults, const FVector& InForwardsDirection);

};
