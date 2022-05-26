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
 * Describes a scene cast that uses speed. Since we have multible speed-related queries, this data is needed by each of them.
 */
USTRUCT()
struct HELPERLIBRARIES_API FSpeedSceneCastInfo
{
	GENERATED_BODY()

	FSpeedSceneCastInfo()
		: CollisionShapeCasted(FCollisionShape())
		, CollisionShapeCastedRotation(FQuat::Identity)
		, CastDirection(FVector::ZeroVector)
		, StartLocation(FVector::ZeroVector)
		, StartSpeed(0.f)
		, StopLocation(FVector::ZeroVector)
		, StopSpeed(0.f)
		, DistanceToStop(0.f)
		, TimeAtStop(0.f)
	{
	}

	/** The shape we used for the query (line trace if ECollisionShape::Line) */
	FCollisionShape CollisionShapeCasted;
	/** Casted shape's rotation */
	FQuat CollisionShapeCastedRotation;
	/** The direction casted in */
	FVector CastDirection;
	/** The location where this scene cast began */
	FVector StartLocation;
	/** The speed this scene cast started with */
	float StartSpeed;
	/** The location where we were stopped */
	FVector StopLocation;
	/** The speed when we stopped */
	float StopSpeed;
	/** Distance from StartLocation to StopLocation */
	float DistanceToStop;
	/** The time where we were stopped */
	float TimeAtStop;
};

/**
 * Struct describing a PenetrationSceneCastWithExitHitsUsingSpeed()
 */
USTRUCT()
struct HELPERLIBRARIES_API FPenetrationSceneCastWithExitHitsUsingSpeedResult
{
	GENERATED_BODY()

	FPenetrationSceneCastWithExitHitsUsingSpeedResult()
		: SpeedSceneCastInfo(FSpeedSceneCastInfo())
		, HitResults(TArray<FShooterHitResult>())
	{
	}

	/** Info about the scene cast that uses speed */
	FSpeedSceneCastInfo SpeedSceneCastInfo;
	/** Hit results in this scene cast */
	TArray<FShooterHitResult> HitResults;


	/** Draws line representing this scene cast, representing speed in color */
	void DrawSpeedDebugLine(const UWorld* InWorld, const float InInitialSpeed, const bool bInPersistentLines = false, const float InLifeTime = -1.f, const uint8 InDepthPriority = 0, const float InThickness = 0.f, const float InSegmentsLength = 10.f, const float InSegmentsSpacingLength = 0.f, const FLinearColor& InFullSpeedColor = FLinearColor::Green, const FLinearColor& InNoSpeedColor = FLinearColor::Red) const;
	/** Draws text representing this scene cast, indicating speed at significant points */
	void DrawSpeedDebugText(const UWorld* InWorld, const float InInitialSpeed, const float InLifeTime = -1.f, const FLinearColor& InFullSpeedColor = FLinearColor::Green, const FLinearColor& InNoSpeedColor = FLinearColor::Red) const;
	/** Draws casted collision shape at significant points */
	void DrawCollisionShapeDebug(const UWorld* InWorld, const float InInitialSpeed, const bool bInPersistentLines = false, const float InLifeTime = -1.f, const uint8 InDepthPriority = 0, const float InThickness = 0.f, const FLinearColor& InFullSpeedColor = FLinearColor::Green, const FLinearColor& InNoSpeedColor = FLinearColor::Red) const;

	static FLinearColor GetDebugColorForSpeed(const float InSpeed, const float InInitialSpeed, const FLinearColor& InFullSpeedColor = FLinearColor::Green, const FLinearColor& InNoSpeedColor = FLinearColor::Red);
};
/**
 * Struct describing a RicochetingPenetrationSceneCastWithExitHitsUsingSpeed()
 */
USTRUCT()
struct HELPERLIBRARIES_API FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult
{
	GENERATED_BODY()

	FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult()
		: SpeedSceneCastInfo(FSpeedSceneCastInfo())
		, PenetrationSceneCastWithExitHitsUsingSpeedResults(TArray<FPenetrationSceneCastWithExitHitsUsingSpeedResult>())
	{
	}

	/** Info about the scene cast that uses speed */
	FSpeedSceneCastInfo SpeedSceneCastInfo;
	/** Penetration with speed scene casts. The initial and ricochets. */
	TArray<FPenetrationSceneCastWithExitHitsUsingSpeedResult> PenetrationSceneCastWithExitHitsUsingSpeedResults;


	/** Drawn representation of this query */
	void DrawFullDebug(const UWorld* InWorld, const float InInitialSpeed, const bool bInPersistentLines = false, const float InLifeTime = -1.f, const uint8 InDepthPriority = 0, const float InThickness = 0.f, const float InSegmentsLength = 10.f, const float InSegmentsSpacingLength = 0.f, const FLinearColor& InFullSpeedColor = FLinearColor::Green, const FLinearColor& InNoSpeedColor = FLinearColor::Red) const;
	/** Draws text representing this scene cast, indicating speed at significant points */
	void DrawSpeedDebugText(const UWorld* InWorld, const float InInitialSpeed, const float InLifeTime = -1.f, const FLinearColor& InFullSpeedColor = FLinearColor::Green, const FLinearColor& InNoSpeedColor = FLinearColor::Red) const;
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
	 * @param  InInitialSpeed            Initial speed of the scene cast.
	 * @param  InOutPerCmSpeedNerfStack  Stack of values that nerf the hypothetical bullet's speed per cm. Top of stack represents the most recent nerf (in penetration terminology, the most recent/inner object currently being penetrated).
	 * @param  InWorld                   The world to scene cast in
	 * @param  OutResult                 Struct that fully describes this query
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
	static FShooterHitResult* PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& InCollisionResponseParams = FCollisionResponseParams::DefaultResponseParam,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable = UBFL_CollisionQueryHelpers::DefaultIsHitImpenetrable);
	static FShooterHitResult* PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& InCollisionResponseParams = FCollisionResponseParams::DefaultResponseParam,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable = UBFL_CollisionQueryHelpers::DefaultIsHitImpenetrable);
	//  END Custom query


	//  BEGIN Custom query
	/**
	 * Penetrating scene cast that can ricochet, using PenetrationSceneCastWithExitHitsUsingSpeed() for each cast.
	 *
	 * @param  OutResult                 Struct that fully describes this query
	 * @param  InStart                   Start location of the scene cast
	 * @param  InDirection               The direction to scene cast
	 * @param  InDistanceCap             The max distance to travel (performance wise, length of the cast will be this large and get smaller as we travel from ricochets)
	 * @param  GetRicochetSpeedNerf      TFunction where caller indicates speed nerf to apply when hitting a ricochetable hit
	 * @param  IsHitRicochetable         TFunction where caller indicates whether we should ricochet off of the HitResult
	 */
	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& InCollisionResponseParams = FCollisionResponseParams::DefaultResponseParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = DefaultGetRicochetSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable = DefaultIsHitRicochetable);
	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& InCollisionResponseParams = FCollisionResponseParams::DefaultResponseParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = DefaultGetPenetrationSpeedNerf,
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = DefaultGetRicochetSpeedNerf,
		const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable = DefaultIsHitRicochetable);
	//  END Custom query


private:
	static float NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm);
};
