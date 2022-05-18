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
struct HELPERLIBRARIES_API FBulletHit : public FExitAwareHitResult
{
	GENERATED_BODY()

	FBulletHit()
		: FExitAwareHitResult()
		, RicochetNumber(0)
		, bIsRicochet(false)
		, Speed(0.f)
	{
	}
	FBulletHit(const FExitAwareHitResult& HitResult)
		: FExitAwareHitResult(HitResult)
		, RicochetNumber(0)
		, bIsRicochet(false)
		, Speed(0.f)
	{
	}

	float BulletHitDistance;
	int32 RicochetNumber;
	uint8 bIsRicochet : 1;
	float Speed;
};

/**
 * 
 */
USTRUCT()
struct HELPERLIBRARIES_API FScanResult
{
	GENERATED_BODY()

	FScanResult()
		: BulletHits(TArray<FBulletHit>())
		, BulletStart(FVector::ZeroVector)
		, BulletEnd(FVector::ZeroVector)
		, InitialBulletSpeed(0.f)
	{
	}

	TArray<FBulletHit> BulletHits;
	FVector BulletStart;
	FVector BulletEnd;
	float InitialBulletSpeed;

	void DebugScan(const UWorld* InWorld) const;
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
	static FBulletHit* PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);
	static FBulletHit* PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);


	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitRicochetable = nullptr);
	static void RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam, const int32 InRicochetCap = -1,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitRicochetable = nullptr);

private:
	static float NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm);
};
