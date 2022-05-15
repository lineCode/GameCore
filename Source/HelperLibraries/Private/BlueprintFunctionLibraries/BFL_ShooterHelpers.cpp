// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "DrawDebugHelpers.h"



FVector UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialBulletSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, TArray<FBulletHit>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	float CurrentSpeed = InInitialBulletSpeed;

	TArray<FExitAwareHitResult> HitResults;
	UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(InWorld, HitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable, true);


	const FVector SceneCastDirection = (InEnd - InStart).GetSafeNormal();

	// For this segment (from TraceStart to the first hit), apply speed nerfs and see if we stopped
	{
		// Calculate this segment's distance
		float SegmentDistance;
		if (HitResults.IsValidIndex(0))
		{
			SegmentDistance = HitResults[0].Distance;
		}
		else
		{
			// Traced into thin air - so use the distance of the whole trace
			SegmentDistance = FVector::Distance(InStart, InEnd);
		}

		// Calculate how much speed per cm we should be taking away for this segment
		// Accumulate all of the speed nerfs from the InOutSpeedNerfStack
		float SpeedToTakeAwayPerCm = 0.f;
		for (const float& SpeedNerf : InOutSpeedNerfStack)
		{
			SpeedToTakeAwayPerCm += SpeedNerf;
		}

		// If the bullet stopped in this segment, stop adding further hits and return the stop location
		const float TraveledThroughDistance = NerfSpeedPerCm(CurrentSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
		if (CurrentSpeed <= 0.f)
		{
			return InStart + (SceneCastDirection * TraveledThroughDistance);
		}
	}

	// Add found hit results to the OutHits
	OutHits.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of speed
	for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
	{
		if (HitResults[i].bStartPenetrating)
		{
			// Initial overlaps would mess up our SpeedNerfStack stack so skip it
			// Btw this is only a thing for simple collision queries
			continue;
		}

		// Add this hit to our bullet hits
		FBulletHit& AddedBulletHit = OutHits.Add_GetRef(HitResults[i]);
		AddedBulletHit.Speed = CurrentSpeed;

		
		// Update the InOutSpeedNerfStack with this hit
		if (AddedBulletHit.bIsExitHit == false)		// Add new nerf if we are entering something
		{
			InOutSpeedNerfStack.Push(GetPenetrationSpeedNerf(AddedBulletHit));
		}
		else										// Remove most recent nerf if we are exiting something
		{
			const int32 IndexOfNerfThatWeAreExiting = InOutSpeedNerfStack.FindLast(GetPenetrationSpeedNerf(AddedBulletHit));

			if (IndexOfNerfThatWeAreExiting != INDEX_NONE)
			{
				InOutSpeedNerfStack.RemoveAt(IndexOfNerfThatWeAreExiting);
			}
			else
			{
				UE_LOG(LogShooterHelpers, Error, TEXT("%s() Bullet exited a penetration nerf that was never entered. This means that the bullet started from within a collider. We can't account for that object's penetration nerf which means our speed values for the bullet scan will be wrong. Make sure to not allow player to start shot from within a colider. Hit Actor: [%s]. ALSO this could've been the callers fault by not having consistent speed nerfs for entrances and exits"), ANSI_TO_TCHAR(__FUNCTION__), GetData(AddedBulletHit.GetActor()->GetName()));
			}
		}

		// For this segment (from this hit to the next hit), apply speed nerfs and see if we stopped
		{
			// Calculate this segment's distance
			float SegmentDistance;
			if (HitResults.IsValidIndex(i + 1))
			{
				// Get distance from this hit to the next hit
				SegmentDistance = (HitResults[i + 1].Distance - AddedBulletHit.Distance); // distance from [i] to [i + 1]
			}
			else
			{
				// Get distance from this hit to trace end
				const float SceneCastDistance = UBFL_HitResultHelpers::GetTraceLengthFromHit(AddedBulletHit);
				SegmentDistance = (SceneCastDistance - AddedBulletHit.Distance);
			}

			// Calculate how much speed per cm we should be taking away for this segment
			// Accumulate all of the speed nerfs from the InOutSpeedNerfStack
			float SpeedToTakeAwayPerCm = 0.f;
			for (const float& SpeedNerf : InOutSpeedNerfStack)
			{
				SpeedToTakeAwayPerCm += SpeedNerf;
			}

			// If the bullet stopped in this segment, stop adding further hits and return the stop location
			const float TraveledThroughDistance = NerfSpeedPerCm(CurrentSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
			if (CurrentSpeed <= 0.f)
			{
				return AddedBulletHit.Location + (SceneCastDirection * TraveledThroughDistance);
			}
		}
	}

	return InEnd;
}
FVector UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialBulletSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, TArray<FBulletHit>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> SpeedNerfStack;
	SpeedNerfStack.Add(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InInitialBulletSpeed, SpeedNerfStack, InWorld, OutHits, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
}
//FVector UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialBulletSpeed, const UWorld* InWorld, TArray<FBulletHit>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
//	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
//	const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
//{
//	TArray<float> SpeedNerfStack;
//	return PenetrationSceneCastWithExitHitsUsingSpeed(InInitialBulletSpeed, SpeedNerfStack, InWorld, OutHits, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
//}

float UBFL_ShooterHelpers::NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm)
{
	const float SpeedToTakeAway = (InNerfPerCm * InDistanceToTravel);
	const float TraveledThroughRatio = (InOutSpeed / SpeedToTakeAway);
	const float TraveledThroughDistance = (TraveledThroughRatio * InDistanceToTravel);
	InOutSpeed -= SpeedToTakeAway;

	return TraveledThroughDistance;
}

void FScanResult::DebugScan(const UWorld* InWorld) const
{
#if ENABLE_DRAW_DEBUG
	const float DebugLifeTime = 5.f;
	const FColor TraceColor = FColor::Blue;
	const FColor HitColor = FColor::Red;

	for (const FHitResult& Hit : BulletHits)
	{
		DrawDebugLine(InWorld, Hit.TraceStart, Hit.Location, TraceColor, false, DebugLifeTime);
		DrawDebugPoint(InWorld, Hit.ImpactPoint, 10.f, HitColor, false, DebugLifeTime);
	}

	if (BulletHits.Num() > 0)
	{
		DrawDebugLine(InWorld, BulletHits.Last().Location, BulletEnd, TraceColor, false, DebugLifeTime);
	}
	else
	{
		DrawDebugLine(InWorld, BulletStart, BulletEnd, TraceColor, false, DebugLifeTime);
	}
#endif // ENABLE_DRAW_DEBUG
}
