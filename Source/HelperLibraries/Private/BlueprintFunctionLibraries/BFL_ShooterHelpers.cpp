// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "DrawDebugHelpers.h"



bool UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<FExitAwareHitResult> HitResults;
	bool bHitImpenetrableHit = UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(InWorld, HitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable, true);


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
			// We casted into thin air - so use the distance of the whole scene cast
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
		const float TraveledThroughDistance = NerfSpeedPerCm(InOutSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
		if (InOutSpeed < 0.f)
		{
			OutScanResult.BulletEnd = InStart + (SceneCastDirection * TraveledThroughDistance);
			return false;
		}
	}

	if (InOutSpeed > 0.f)
	{
		// Add found hit results to the OutScanResult
		OutScanResult.BulletHits.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of speed
		for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
		{
			if (HitResults[i].bStartPenetrating)
			{
				// Initial overlaps would mess up our SpeedNerfStack stack so skip it
				// Btw this is only a thing for simple collision queries
				continue;
			}

			// Add this hit to our bullet hits
			FBulletHit& AddedBulletHit = OutScanResult.BulletHits.Add_GetRef(HitResults[i]);
			AddedBulletHit.Speed = InOutSpeed;

			if (bHitImpenetrableHit && i == HitResults.Num() - 1)
			{
				// Stop - don't calculate penetration nerfing on impenetrable hit
				OutScanResult.BulletEnd = AddedBulletHit.Location;
				return true;
			}

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
				const float TraveledThroughDistance = NerfSpeedPerCm(InOutSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
				if (InOutSpeed < 0.f)
				{
					OutScanResult.BulletEnd = AddedBulletHit.Location + (SceneCastDirection * TraveledThroughDistance);
					return false;
				}
			}
		}
	}

	// InOutSpeed made it past every nerf
	OutScanResult.BulletEnd = InEnd;
	return false;
}
bool UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> SpeedNerfStack;
	SpeedNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, SpeedNerfStack, InWorld, OutScanResult, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
}

void UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutSpeedNerfStack, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunction<bool(const FHitResult&)>& IsHitRicochetable)
{
	OutScanResult.BulletStart = InStart;
	OutScanResult.InitialBulletSpeed = InOutSpeed;

	FVector SceneCastStart = InStart;
	FVector SceneCastDirection = InDirection;
	float RemainingDistance = InDistanceCap;

	// The first iteration of this loop is the initial scene cast and the rest of the iterations are ricochet scene casts
	for (int32 RicochetNumber = 0; (RicochetNumber <= InRicochetCap || InRicochetCap == -1); ++RicochetNumber)
	{
		const FVector SceneCastEnd = SceneCastStart + (SceneCastDirection * RemainingDistance);

		FScanResult ScanResult;
		const bool bHitRicochetableHit = PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, InOutSpeedNerfStack, InWorld, ScanResult, SceneCastStart, SceneCastEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitRicochetable);

		// Set BulletHit data
		{
			// Give data to our bullet hits for this scene cast
			for (FBulletHit& BulletHit : ScanResult.BulletHits)
			{
				const float DistanceUpUntilThisSceneCast = (InDistanceCap - RemainingDistance);

				BulletHit.RicochetNumber = RicochetNumber;
				BulletHit.BulletHitDistance = DistanceUpUntilThisSceneCast + BulletHit.Distance;
			}

			// Ricochet speed nerf and ricochet hit info
			if (bHitRicochetableHit)
			{
				FBulletHit& RicochetableHit = ScanResult.BulletHits.Last();
				RicochetableHit.bIsRicochet = true;
			}
		}

		// Apply ricochet speed nerf
		if (bHitRicochetableHit)
		{
			FBulletHit& RicochetableHit = ScanResult.BulletHits.Last();
			const float RicochetSpeedNerf = GetRicochetSpeedNerf(RicochetableHit);
			InOutSpeed -= RicochetSpeedNerf;
		}


		// Add to our output
		OutScanResult.BulletHits.Append(ScanResult.BulletHits);

		// Check if we should follow through with the next trace
		if (InOutSpeed < 0.f)
		{
			OutScanResult.BulletEnd = ScanResult.BulletEnd;
			return;
		}

		if (!bHitRicochetableHit)
		{
			// Nothing to ricochet off of
			OutScanResult.BulletEnd = SceneCastEnd;
			return;
		}

		// SETUP FOR OUR NEXT SCENE CAST

		FBulletHit& RicochetableHit = ScanResult.BulletHits.Last();

		// Take away the distance traveled of this scene cast from our RemainingDistance
		RemainingDistance -= RicochetableHit.Distance;

		// Edge case: if we ran out of distance exactly when we hit a ricochet
		if (RemainingDistance == 0.f)
		{
			OutScanResult.BulletEnd = RicochetableHit.Location;
			return;
		}

		// Reflect our SceneCastDirection off of the ricochetable surface and calculate SceneCastStart for next ricochet scene cast
		SceneCastDirection = SceneCastDirection.MirrorByVector(RicochetableHit.ImpactNormal);
		SceneCastStart = RicochetableHit.Location + (SceneCastDirection * UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding);
	}

	
}

void UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FScanResult& OutScanResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunction<bool(const FHitResult&)>& IsHitRicochetable)
{
	TArray<float> SpeedNerfStack;
	SpeedNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, SpeedNerfStack, InWorld, OutScanResult, InStart, InDirection, InDistanceCap, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InRicochetCap, GetPenetrationSpeedNerf, GetRicochetSpeedNerf, IsHitRicochetable);
}

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
