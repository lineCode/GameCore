// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PhysicalMaterials\PhysicalMaterial.h"
#include "Kismet/KismetMathLibrary.h"

#include "BFL_CollisionQueryHelpers.generated.h"



/**
 * Represents a segment in the World and holds a stack of Phys Mats that are within the two end points.
 */
struct FTraceSegment
{
	FTraceSegment(const FVector& InStartPoint, const FVector& InEndPoint)
	{
		// Set points
		StartPoint = InStartPoint;
		EndPoint = InEndPoint;

		// Cache our calculations
		SegmentDistance = FVector::Distance(StartPoint, EndPoint);
		TraceDir = (EndPoint - StartPoint).GetSafeNormal();
	}

	/** This is the stack of Physical Materials that are enclosed in this Segment. Top of the stack is the most inner (most recent) Phys Mat */
	TArray<UPhysicalMaterial*> PhysMaterials;

	FVector GetStartPoint() const { return StartPoint; }
	FVector GetEndPoint() const { return EndPoint; }

	float GetSegmentDistance() const { return SegmentDistance; }
	FVector GetTraceDir() const { return TraceDir; }

private:
	// Our segment points
	FVector StartPoint;
	FVector EndPoint;

	// Cached calculations of our segment
	float SegmentDistance;
	FVector TraceDir;
};

struct FExitAwareHitResult : FHitResult
{
	FExitAwareHitResult(const FHitResult& HitResult)
		: FHitResult(HitResult)
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
	 *  @param  InTraceChannel            The 'channel' that this ray is in, used to determine which components to hit. Unlike standard UE traces, blocking hits for this channel don't stop the trace, but is instead only used for preserving the CollisionResponse of each hit.
	 *  @param  InCollisionQueryParams    Additional parameters used for the trace
	 *  @param  ShouldStopAtHit           TFunction where caller indicates whether provided HitResult should stop the trace
	 *  @return TRUE if hit and stopped at an impenetrable hit.
	 */
	static bool LineTraceMultiByChannelWithPenetrations(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const TFunctionRef<bool(const FHitResult&)>& ShouldStopAtHit = [](const FHitResult&) { return false; });

	/**
	 *  Line trace multi with penetrations that outputs entrance and exit hits in order of the forward tracing direction
	 *  
	 *  @param  OutHits                          Array of entrance and exit hits (overlap and blocking) that were found until ShouldStopAtHit condition is met
	 *  @param  bUseBackwardsTraceOptimization   Only use this if you're not starting the trace inside of geometry, otherwise the exits of any gemometry you're starting inside of may not be found. However, you could possibly still get away with this optimization if you are doing a very lengthy trace, because you are more likely to hit an entrance past the exit of the geometry that you started in. If true, will minimize the backwards tracing distance to go no further than the exit of the furthest entrance.
	 *  @return TRUE if hit and stopped at an impenetrable hit.
	 */
	static bool ExitAwareLineTraceMultiByChannelWithPenetrations(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const TFunctionRef<bool(const FHitResult&)>& ShouldStopAtHit = [](const FHitResult&) { return false; }, const bool bUseBackwardsTraceOptimization = false);


	/**
	 * Gets the direction from the Location to the point that AimDir is looking at.
	 * Useful for having a weapon's muzzle aim towards the Player's look point.
	 */
	static FVector GetLocationAimDirection(const UWorld* InWorld, const FCollisionQueryParams& Params, const FVector& AimPoint, const FVector& AimDir, const float& MaxRange, const FVector& Location);


	/**
	 * BuildTraceSegments Important notes:
	 * This gathers data on the geometry level. If there are faces with normals pointing at an incomming trace, it will hit. This can get really (pointlessly) expensive if a model has lots of faces not visible and inside the mesh
	 * since it will penetrate those as well. I think in most cases it's pointless to do penetrations on those inner geometry parts, but we are just doing that for now. We could make an implementation that runs on the section level
	 * instead of the geometry level (using HitResult.GetComponent()->GetMaterialFromCollisionFaceIndex(HitResult.FaceIndex); ). We were just kinda too lazy to do this. We did this in a pervious commit but changed the system.
	 * This is mainly a system for used for when we are using complex collision. If any hit results that are passed in is a Hit Result from a component's collider that uses
	 * CTF_UseSimpleAsComplex, things can get inacurrate, but only if that collider's mesh has multible material slots. This is because once we hit any simple collider,we ignore the rest of it so we don't get stuck
	 * inside it (distance being 0 every time).	Another note is that skeletal mesh bones are all a part of 1 component (skeletal mesh component), and each of the bones use simple collision.
	 * This means that if you hit a skeletal mesh bone, it won't just only ignore that specific bone. It will ignore all of the bones in that skeletal mesh component (since it just ignores the skeletal mesh component).
	 * So when using this against skeletal meshes, only one bone will be hit.
	 * Another thing is if the first fwd hit result it already inside some geometry, then the function won't be aware of that and the stack of physical materials won't be fully accurate.
	*/
	static void BuildTraceSegments(OUT TArray<FTraceSegment>& OutTraceSegments, const TArray<FHitResult>& FwdBlockingHits, const UWorld* World, const FCollisionQueryParams& TraceParams, const TEnumAsByte<ECollisionChannel> TraceChannel);
	static void BuildTraceSegments(OUT TArray<FTraceSegment>& OutTraceSegments, const TArray<FHitResult>& FwdBlockingHits, const FVector& FwdEndLocation, const UWorld* World, const FCollisionQueryParams& TraceParams, const TEnumAsByte<ECollisionChannel> TraceChannel);
	
	
	/**
	 * 
	 */
	static void LineTracePenetrateBetweenPoints(OUT TArray<FHitResult>& OutHitResults, const UWorld* World, const FVector& Start, const FVector& End, const ECollisionChannel TraceChannel, const FCollisionQueryParams& Params);

};
