// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "BFL_ShooterHelpers.generated.h"



/**
 * 
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


	float TraveledDistanceBeforeThisTrace;
	int32 RicochetNumber;
	uint8 bIsRicochet : 1;
	float Speed;

	float GetTotalDistanceTraveled() const
	{
		return TraveledDistanceBeforeThisTrace + Distance;
	}
};

/**
 * 
 */
USTRUCT()
struct HELPERLIBRARIES_API FShootResult
{
	GENERATED_BODY()

	FShootResult()
		: ShooterHits(TArray<FShooterHitResult>())
		, StartLocation(FVector::ZeroVector)
		, EndLocation(FVector::ZeroVector)
		, InitialSpeed(0.f)
	{
	}

	TArray<FShooterHitResult> ShooterHits;
	FVector StartLocation;
	FVector EndLocation;
	float InitialSpeed;

	void DebugShot(const UWorld* InWorld) const;
};


/**
 * Useful shooter related functionality
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_ShooterHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 
	 */
	static FShooterHitResult* PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);
	static FShooterHitResult* PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);


	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitRicochetable = nullptr);
	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitRicochetable = nullptr);

private:
	static float NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm);
};
