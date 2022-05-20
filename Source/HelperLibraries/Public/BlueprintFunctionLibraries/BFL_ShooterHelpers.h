// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "BFL_ShooterHelpers.generated.h"



/**
 * Aditional hit info required for shooter queries
 */
USTRUCT()
struct HELPERLIBRARIES_API FShooterHitResult : public FExitAwareHitResult
{
	GENERATED_BODY()

	FShooterHitResult()
		: FExitAwareHitResult()
		, TraveledDistanceBeforeThisTrace(0.f)
		, RicochetNumber(0)
		, bIsRicochet(false)
		, Speed(0.f)
	{
	}
	FShooterHitResult(const FExitAwareHitResult& HitResult)
		: FExitAwareHitResult(HitResult)
		, TraveledDistanceBeforeThisTrace(0.f)
		, RicochetNumber(0)
		, bIsRicochet(false)
		, Speed(0.f)
	{
	}

	/** Represents the distance the hypothetical bullet traveled before getting to this specific trace */
	float TraveledDistanceBeforeThisTrace;
	/** How many times the hypothetical bullet ricocheted before this hit */
	int32 RicochetNumber;
	/** We hit a ricochetable surface */
	uint8 bIsRicochet : 1;
	/** Current speed at the location of hit */
	float Speed;

	/** Gets the total distance the hypothetical bullet traveled until this hit's location */
	float GetTotalDistanceTraveledToThisHit() const
	{
		return TraveledDistanceBeforeThisTrace + Distance;
	}
};

/**
 * Struct describing a scene cast and holds hit results
 */
USTRUCT()
struct HELPERLIBRARIES_API FSceneCastResult
{
	GENERATED_BODY()

	FSceneCastResult()
		: HitResults(TArray<FShooterHitResult>())
		, StartLocation(FVector::ZeroVector)
		, EndLocation(FVector::ZeroVector)
		, StartDirection(FVector::ZeroVector)
		, LengthFromStartToEnd(0.f)
	{
	}

	/** Hit results in this scene cast */
	TArray<FShooterHitResult> HitResults;
	/** The location where this scene cast began */
	FVector StartLocation;
	/** The location where this scene cast stopped */
	FVector EndLocation;
	/** The direction casted in */
	FVector StartDirection;
	/** Distance from StartLocation to EndLocation */
	float LengthFromStartToEnd;
};

/**
 * Shooting related collision queries
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_ShooterHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static const TFunctionRef<float(const FHitResult&)>& DefaultGetPenetrationSpeedNerf;
	static const TFunctionRef<float(const FHitResult&)>& DefaultGetRicochetSpeedNerf;
	static const TFunctionRef<bool(const FHitResult&)>& DefaultIsHitRicochetable;


	//  BEGIN Custom query
	/**
	 * Given an initial speed, perform a scene cast, applying nerfs to the hypothetical bullet as it penetrates through blocking hits.
	 *
	 * @param  InOutSpeed                Initial speed of the scene cast. Capable of going negative. However, any casting for hits ends at a speed of 0.
	 * @param  InOutPerCmSpeedNerfStack  Stack of values that nerf the hypothetical bullet's speed per cm. Top of stack represents the most recent nerf (in penetration terminology, the most recent/inner object currently being penetrated).
	 * @param  InWorld                   The world to scene cast in
	 * @param  OutSceneCastResult        Struct that describes the scene cast and holds the hit results
	 * @param  InStart                   Start location of the scene cast
	 * @param  InEnd                     End location of the scene cast
	 * @param  InRotation                Rotation of the collision shape (needed for sweeps)
	 * @param  InTraceChannel            The trace channel for this scene cast
	 * @param  InCollisionShape          Generic collision shape for sweeps/traces (FCollisionShape::LineShape for a line trace)
	 * @param  InCollisionQueryParams    Additional parameters used for the scene cast
	 * @param  GetPenetrationSpeedNerf   TFunction where caller indicates a per cm speed nerf to apply when entering geometry given a hit
	 * @param  IsHitImpenetrable         TFunction where caller indicates whether provided HitResult should stop us
	 * @return The impenetrable hit if we hit one
	 */
	static FShooterHitResult* PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FSceneCastResult& OutSceneCastResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable = UBFL_CollisionQueryHelpers::DefaultIsHitImpenetrable);
	static FShooterHitResult* PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FSceneCastResult& OutSceneCastResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable = UBFL_CollisionQueryHelpers::DefaultIsHitImpenetrable);
	//  END Custom query


	//  BEGIN Custom query
	/**
	 * Penetrating scene cast that can ricochet, using PenetrationSceneCastWithExitHitsUsingSpeed() for each cast.
	 *
	 * @param  OutSceneCastResults       Array of structs that describe the scene casts and hold the hit results
	 * @param  InStart                   Start location of the scene cast
	 * @param  InDirection               The direction to scene cast
	 * @param  InDistanceCap             The max distance to travel (performance wise, length of the cast will be this large and get smaller as we travel from ricochets)
	 * @param  GetRicochetSpeedNerf      TFunction where caller indicates speed nerf to apply when hitting a ricochetable hit
	 * @param  IsHitRicochetable         TFunction where caller indicates whether we should ricochet off of the HitResult
	 */
	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, TArray<FSceneCastResult>& OutSceneCastResults, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = DefaultGetRicochetSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable = DefaultIsHitRicochetable);
	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, TArray<FSceneCastResult>& OutSceneCastResults, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = DefaultGetRicochetSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable = DefaultIsHitRicochetable);
	//  END Custom query

private:
	static float NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm);
};
