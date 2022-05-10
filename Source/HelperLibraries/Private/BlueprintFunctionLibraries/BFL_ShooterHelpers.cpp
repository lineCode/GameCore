// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "DrawDebugHelpers.h"



const float UBFL_ShooterHelpers::TraceStartWallAvoidancePadding = .01f;

void UBFL_ShooterHelpers::ScanWithLineTracesUsingSpeed(FScanResult& OutScanResult, const FVector& InScanStart, const FVector& InScanDirection, const float InDistanceCap, const UWorld* InWorld, const ECollisionChannel InTraceChannel, FCollisionQueryParams CollisionQueryParams, const int32 InMaxPenetrations, const int32 InMaxRicochets, const float InInitialBulletSpeed, const float InRangeFalloffNerf,
	const TFunction<bool(const FHitResult&)>& ShouldRicochetOffOf,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf)
{
	OutScanResult.BulletStart = InScanStart;
	OutScanResult.InitialBulletSpeed = InInitialBulletSpeed;

	float BulletSpeed = InInitialBulletSpeed;
	TArray<float> PenetrationNerfStack;

	FVector TraceStart = InScanStart;
	FVector TraceDirection = InScanDirection;
	float RemainingScanDistance = InDistanceCap;

	// The first iteration of this loop is the initial trace and the rest of the iterations are ricochet traces
	int32 PenetrationNumber = 0;
	for (int32 RicochetNumber = 0; (RicochetNumber <= InMaxRicochets || InMaxRicochets == -1); ++RicochetNumber)
	{
		TArray<FExitAwareHitResult> TraceHitResults;
		const FVector TraceEnd = TraceStart + (TraceDirection * RemainingScanDistance);

		// Penetration line trace that stops at a user defined ricochetable surface
		const bool bHitRicochetableSurface = UBFL_CollisionQueryHelpers::ExitAwareLineTraceMultiByChannelWithPenetrations(InWorld, TraceHitResults, TraceStart, TraceEnd, InTraceChannel, CollisionQueryParams, ShouldRicochetOffOf, true);

		// If the caller is using speed, apply speed nerfs and see if we stopped (from TraceStart to the first hit)
		if (InInitialBulletSpeed != -1)
		{
			float DistanceToFirstHit;
			if (TraceHitResults.Num() > 0)
			{
				DistanceToFirstHit = TraceHitResults[0].Distance;
			}
			else
			{
				// Traced into thin air - so use the distance of the whole trace
				DistanceToFirstHit = RemainingScanDistance;
			}

			// Range falloff nerf
			float SpeedToTakeAwayPerCm = InRangeFalloffNerf;

			// Accumulate all of the speed nerfs from the PenetrationNerfStack
			// This can only have items at this point for ricochet traces because this system allows ricochet traces to start inside of penetrations
			for (const float& PenetrationNerf : PenetrationNerfStack)
			{
				SpeedToTakeAwayPerCm += PenetrationNerf;
			}

			// Apply the speed nerf
			const float TraveledDistance = NerfSpeedPerCm(BulletSpeed, DistanceToFirstHit, SpeedToTakeAwayPerCm);
			if (BulletSpeed <= 0.f)
			{
				// Ran out of speed. Get the point stopped at
				OutScanResult.BulletEnd = TraceStart + (TraveledDistance * TraceDirection);
				return;
			}
		}

		// Add found hit results to the return value
		OutScanResult.BulletHits.Reserve(OutScanResult.BulletHits.Num() + TraceHitResults.Num()); // assume that we will add all of the hits. But if caller is using speed, there may end up being reserved space that goes unused if we run out of speed
		for (int32 i = 0; i < TraceHitResults.Num(); ++i)
		{
			if (TraceHitResults[i].bStartPenetrating)
			{
				// Initial overlaps would mess up our PenetrationNerfStack stack so skip it
				// Btw this is only a thing for non-bTraceComplex queries
				continue;
			}

			// Add this hit to our bullet hits
			FBulletHit& AddedBulletHit = OutScanResult.BulletHits.Add_GetRef(TraceHitResults[i]);
			AddedBulletHit.RicochetNumber = RicochetNumber;
			AddedBulletHit.bIsRicochet = ShouldRicochetOffOf(AddedBulletHit);
			AddedBulletHit.Speed = BulletSpeed;
			AddedBulletHit.BulletHitDistance = (InDistanceCap - RemainingScanDistance) + AddedBulletHit.Distance; // RemainingScanDistance is the distance of the current trace and all traces after TODO: Make this more understandable by creating a DistanceTraveled float. Every trace, add the distance from the start of the trace to the point the bullet stopped at or ricocheted at

			// Handle ricochet hit
			if (AddedBulletHit.bIsRicochet)
			{
				// If the caller is using speed
				if (InInitialBulletSpeed != -1)
				{
					// Apply speed nerf and see if we stopped
					BulletSpeed -= GetRicochetSpeedNerf(AddedBulletHit);
					if (BulletSpeed <= 0.f)
					{
						// Ran out of speed. Get the point stopped at
						OutScanResult.BulletEnd = AddedBulletHit.Location;
						return;
					}
				}

				// We are a ricochet hit so make us the last hit (forget about the rest of the hits)
				break;
			}

			// If caller is using speed
			if (InInitialBulletSpeed != -1)
			{
				// Update our PenetrationNerfStack with this hit
				{
					if (AddedBulletHit.bIsExitHit == false)
					{
						PenetrationNerfStack.Push(GetPenetrationSpeedNerf(AddedBulletHit));
					}
					else
					{
						const int32 IndexOfNerfThatWeAreExiting = PenetrationNerfStack.FindLast(GetPenetrationSpeedNerf(AddedBulletHit));

						if (IndexOfNerfThatWeAreExiting != INDEX_NONE)
						{
							PenetrationNerfStack.RemoveAt(IndexOfNerfThatWeAreExiting);
						}
						else
						{
							UE_LOG(LogShooterHelpers, Error, TEXT("%s() Bullet exited a penetration nerf that was never entered. This means that the bullet started from within a collider. We can't account for that object's penetration nerf which means our speed values for the bullet scan will be wrong. Make sure to not allow player to start shot from within a colider. Hit Actor: [%s]. ALSO this could've been the callers fault by not having consistent speed nerfs for entrances and exits"), ANSI_TO_TCHAR(__FUNCTION__), GetData(AddedBulletHit.GetActor()->GetName()));
						}
					}
				}


				// Apply speed nerfs and see if we stopped (from this hit to the next hit)
				{
					float DistanceToNextHit;
					if (TraceHitResults.IsValidIndex(i + 1))
					{
						// Get distance from this hit to the next hit
						DistanceToNextHit = (TraceHitResults[i + 1].Distance - AddedBulletHit.Distance); // distance from [i] to [i + 1]
					}
					else
					{
						// Get distance from this hit to trace end
						DistanceToNextHit = (RemainingScanDistance - AddedBulletHit.Distance);
					}

					// Range falloff nerf
					float SpeedToTakeAwayPerCm = InRangeFalloffNerf;

					// Accumulate all of the speed nerfs from the PenetrationNerfStack
					for (const float& PenetrationNerf : PenetrationNerfStack)
					{
						SpeedToTakeAwayPerCm += PenetrationNerf;
					}

					// Apply the speed nerf
					const float TraveledDistance = NerfSpeedPerCm(BulletSpeed, DistanceToNextHit, SpeedToTakeAwayPerCm);
					if (BulletSpeed <= 0.f)
					{
						// Ran out of speed. Get the point stopped at
						OutScanResult.BulletEnd = AddedBulletHit.Location + (TraveledDistance * TraceDirection);
						return;
					}
				}
			}

			// Check if we ran out of penetrations
			if (AddedBulletHit.bBlockingHit)
			{
				if (AddedBulletHit.bIsExitHit == false) // if we are penetrating through an entrance
				{
					++PenetrationNumber;
					if (PenetrationNumber > InMaxPenetrations && InMaxPenetrations != -1)
					{
						return;
					}
				}
			}
		}

		if (!bHitRicochetableSurface)
		{
			// Nothing to ricochet off of
			break;
		}

		// SETUP FOR OUR NEXT TRACE

		// Take away the distance traveled of this trace from our RemainingScanDistance
		RemainingScanDistance -= OutScanResult.BulletHits.Last().Distance; // since we stopped at a ricochet, the last hit is our ricochet hit
		if (RemainingScanDistance <= 0.f)
		{
			break;
		}

		// Reflect our TraceDirection off of the ricochetable surface and calculate TraceStart for next ricochet trace
		TraceDirection = TraceDirection.MirrorByVector(OutScanResult.BulletHits.Last().ImpactNormal);
		TraceStart = OutScanResult.BulletHits.Last().Location + (TraceDirection * TraceStartWallAvoidancePadding);
	}
}

float UBFL_ShooterHelpers::NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm)
{
	const float SpeedToTakeAway = (InNerfPerCm * InDistanceToTravel);
	const float TraveledRatio = (InOutSpeed / SpeedToTakeAway);
	const float TraveledDistance = (InDistanceToTravel * TraveledRatio);
	InOutSpeed -= SpeedToTakeAway;

	return TraveledDistance;
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
