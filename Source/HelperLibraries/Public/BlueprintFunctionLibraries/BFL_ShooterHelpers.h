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
	static FVector PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialBulletSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, TArray<FBulletHit>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);
	static FVector PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialBulletSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, TArray<FBulletHit>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
		const TFunctionRef<float(const FHitResult&)>&GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);
	//static FVector PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialBulletSpeed, const UWorld* InWorld, TArray<FBulletHit>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams = FCollisionQueryParams::DefaultQueryParam,
	//	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
	//	const TFunction<bool(const FHitResult&)>& IsHitImpenetrable = nullptr);

private:
	static float NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm);
};
