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
	 * Performs traces from a given ScanStart into a given ScanDirection with ricochets and penetrations.
	 * Implements the concept of bullet speed. Each ricochet and penetration applies speed nerfs implemented
	 * by optional TFunction parameters.
	 * May want to re-use an array for OutHitResults when calling multiple times to avoid useless allocations and deallocations.
	 *
	 * @param OutScanResult              Output of this function. Includes bullet hit results and scan information.
	 * @param InMaxPenetrations          Max times to penetrate through blocking hits (-1 is unlimited)
	 * @param InMaxRicochets             Max times to ricochet off of owner defined ricochetable hits (-1 is unlimited)
	 * @param InInitialBulletSpeed       Bullet speed that is slowed down by ricochets and penetrations. Bullet stops when we run out of bullet speed.
	 * @param InRangeFalloffNerf         The falloff of range for this bullet per cm
	 * @param ShouldRicochetOffOf        TFunction where caller tells us if a hit result causes a ricochet
	 * @param GetPenetrationSpeedNerf    TFunction for caller to specify the speed nerf PER CENTIMETER of a penetration hit
	 * @param GetRicochetSpeedNerf       TFunction for caller to specify the speed nerf of a ricochet hit
	 */
	static void ScanWithLineTracesUsingSpeed(FScanResult& OutScanResult, const FVector& InScanStart, const FVector& InScanDirection, const float InDistanceCap, const UWorld* InWorld, const ECollisionChannel InTraceChannel, FCollisionQueryParams CollisionQueryParams, const int32 InMaxPenetrations, const int32 InMaxRicochets, const float InInitialBulletSpeed, const float InRangeFalloffNerf,
		const TFunction<bool(const FHitResult&)>& ShouldRicochetOffOf,
		const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; },
		const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; });

private:
	/** Moves our traces' start points in the trace direction by a small amount to ensure we don't get stuck hitting the same object over and over again. This allows us to avoid ignoring the component so that we can hit the same component's geometry again */
	static const float TraceStartWallAvoidancePadding;

	static float NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm);
};
