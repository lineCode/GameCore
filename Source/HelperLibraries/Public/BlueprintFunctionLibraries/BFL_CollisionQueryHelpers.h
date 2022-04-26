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


/**
 * A collection of our custom collision queries
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_CollisionQueryHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
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

	/**
	 * Gets the direction from the Location to the point that AimDir is looking at.
	 * Useful for having a weapon's muzzle aim towards the Player's look point.
	 */
	static FVector GetLocationAimDirection(const UWorld* InWorld, const FCollisionQueryParams& Params, const FVector& AimPoint, const FVector& AimDir, const float& MaxRange, const FVector& Location);
};
