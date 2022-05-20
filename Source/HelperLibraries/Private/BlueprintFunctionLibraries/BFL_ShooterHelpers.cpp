// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "DrawDebugHelpers.h"



const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<bool(const FHitResult&)>& UBFL_ShooterHelpers::DefaultIsHitRicochetable = [](const FHitResult&) { return false; };

//  BEGIN Custom query
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, TArray<FShooterHitResult>& OutShooterHits, FSceneCastInfo& OutSceneCastInfo, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	OutSceneCastInfo.StartLocation = InStart;
	OutSceneCastInfo.StartDirection = (InEnd - InStart).GetSafeNormal();

	TArray<FExitAwareHitResult> HitResults;
	FExitAwareHitResult* ImpenetrableHit = UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(InWorld, HitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable, true);


	const FVector SceneCastDirection = (InEnd - InStart).GetSafeNormal();
	const float SceneCastDistance = HitResults.Num() > 0 ? UBFL_HitResultHelpers::GetTraceLengthFromHit(HitResults.Last(), false) : FVector::Distance(InStart, InEnd);

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
			SegmentDistance = SceneCastDistance;
		}

		// Calculate how much speed per cm we should be taking away for this segment
		// Accumulate all of the speed nerfs from the InOutPerCmSpeedNerfStack
		float SpeedToTakeAwayPerCm = 0.f;
		for (const float& SpeedNerf : InOutPerCmSpeedNerfStack)
		{
			SpeedToTakeAwayPerCm += SpeedNerf;
		}

		// If the bullet stopped in this segment, stop adding further hits and return the stop location
		const float TraveledThroughDistance = NerfSpeedPerCm(InOutSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
		if (InOutSpeed < 0.f)
		{
			OutSceneCastInfo.EndLocation = InStart + (SceneCastDirection * TraveledThroughDistance);
			OutSceneCastInfo.TotalDistanceTraveled = TraveledThroughDistance;
			return nullptr;
		}
	}

	if (InOutSpeed > 0.f)
	{
		// Add found hit results to the OutShooterHits
		OutShooterHits.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of speed
		for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
		{
			// Add this hit to our shooter hits
			FShooterHitResult& AddedShooterHit = OutShooterHits.Add_GetRef(HitResults[i]);
			AddedShooterHit.Speed = InOutSpeed;

			if (HitResults[i].bStartPenetrating)
			{
				// Initial overlaps would mess up our PerCmSpeedNerfStack so skip it
				// Btw this is only a thing for simple collision queries
				UE_LOG(LogShooterHelpers, Verbose, TEXT("%s() Scene cast started inside of something. Make sure to not allow player to shoot staring inside of geometry. We will not consider this hit for the penetration speed nerf stack but it will still be included in the outputed hits. Hit Actor: [%s]."), ANSI_TO_TCHAR(__FUNCTION__), GetData(HitResults[i].GetActor()->GetName()));
				continue;
			}

			if (ImpenetrableHit && &HitResults[i] == ImpenetrableHit)
			{
				// Stop - don't calculate penetration nerfing on impenetrable hit
				OutSceneCastInfo.EndLocation = AddedShooterHit.Location;
				OutSceneCastInfo.TotalDistanceTraveled = AddedShooterHit.Distance;
				return &AddedShooterHit;
			}

			// Update the InOutPerCmSpeedNerfStack with this hit
			if (AddedShooterHit.bIsExitHit == false)		// Add new nerf if we are entering something
			{
				InOutPerCmSpeedNerfStack.Push(GetPenetrationSpeedNerf(AddedShooterHit));
			}
			else										// Remove most recent nerf if we are exiting something
			{
				const int32 IndexOfNerfThatWeAreExiting = InOutPerCmSpeedNerfStack.FindLast(GetPenetrationSpeedNerf(AddedShooterHit));

				if (IndexOfNerfThatWeAreExiting != INDEX_NONE)
				{
					InOutPerCmSpeedNerfStack.RemoveAt(IndexOfNerfThatWeAreExiting);
				}
				else
				{
					UE_LOG(LogShooterHelpers, Error, TEXT("%s() Bullet exited a penetration nerf that was never entered. This must be the callers fault by his GetPenetrationSpeedNerf() not having consistent speed nerfs for entrances and exits. Hit Actor: [%s]."), ANSI_TO_TCHAR(__FUNCTION__), GetData(AddedShooterHit.GetActor()->GetName()));
				}
			}

			// For this segment (from this hit to the next hit), apply speed nerfs and see if we stopped
			{
				// Calculate this segment's distance
				float SegmentDistance;
				if (HitResults.IsValidIndex(i + 1))
				{
					// Get distance from this hit to the next hit
					SegmentDistance = (HitResults[i + 1].Distance - AddedShooterHit.Distance); // distance from [i] to [i + 1]
				}
				else
				{
					// Get distance from this hit to trace end
					SegmentDistance = (SceneCastDistance - AddedShooterHit.Distance);
				}

				// Calculate how much speed per cm we should be taking away for this segment
				// Accumulate all of the speed nerfs from the InOutPerCmSpeedNerfStack
				float SpeedToTakeAwayPerCm = 0.f;
				for (const float& SpeedNerf : InOutPerCmSpeedNerfStack)
				{
					SpeedToTakeAwayPerCm += SpeedNerf;
				}

				// If the bullet stopped in this segment, stop adding further hits and return the stop location
				const float TraveledThroughDistance = NerfSpeedPerCm(InOutSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
				if (InOutSpeed < 0.f)
				{
					OutSceneCastInfo.EndLocation = AddedShooterHit.Location + (SceneCastDirection * TraveledThroughDistance);
					OutSceneCastInfo.TotalDistanceTraveled = AddedShooterHit.Distance + TraveledThroughDistance;
					return nullptr;
				}
			}
		}
	}

	// InOutSpeed made it past every nerf
	OutSceneCastInfo.EndLocation = InEnd;
	OutSceneCastInfo.TotalDistanceTraveled = SceneCastDistance;
	return nullptr;
}
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, TArray<FShooterHitResult>& OutShooterHits, FSceneCastInfo& OutSceneCastInfo, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, PerCmSpeedNerfStack, InWorld, OutShooterHits, OutSceneCastInfo, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
}
//  END Custom query

//  BEGIN Custom query
void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, TArray<FShooterHitResult>& OutShooterHits, FSceneCastInfo& OutSceneCastInfo, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	OutSceneCastInfo.StartLocation = InStart;
	OutSceneCastInfo.StartDirection = InDirection;

	FVector CurrentSceneCastStart = InStart;
	FVector CurrentSceneCastDirection = InDirection;
	float DistanceTraveled = 0.f;

	// The first iteration of this loop is the initial scene cast and the rest of the iterations are ricochet scene casts
	for (int32 RicochetNumber = 0; (RicochetNumber <= InRicochetCap || InRicochetCap == -1); ++RicochetNumber)
	{
		const FVector SceneCastEnd = CurrentSceneCastStart + (CurrentSceneCastDirection * (InDistanceCap - DistanceTraveled));

		TArray<FShooterHitResult> Hits;
		FSceneCastInfo SceneCastInfo;
		FShooterHitResult* RicochetableHit = PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, InOutPerCmSpeedNerfStack, InWorld, Hits, SceneCastInfo, CurrentSceneCastStart, SceneCastEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitRicochetable);
		DistanceTraveled += SceneCastInfo.TotalDistanceTraveled;

		// Set more shooter hit result data
		{
			// Give data to our shooter hits for this scene cast
			for (FShooterHitResult& ShooterHit : Hits)
			{
				ShooterHit.RicochetNumber = RicochetNumber;
				ShooterHit.TraveledDistanceBeforeThisTrace = (DistanceTraveled - SceneCastInfo.TotalDistanceTraveled); // distance up until this scene cast
			}

			// Give data to the ricochet hit
			if (RicochetableHit)
			{
				RicochetableHit->bIsRicochet = true;
			}
		}

		// Apply ricochet speed nerf
		if (RicochetableHit)
		{
			InOutSpeed -= GetRicochetSpeedNerf(*RicochetableHit);
		}


		// Finished with this cast. Add this cast's hit results to our output
		OutShooterHits.Append(Hits);

		// Check if we should end here
		{
			// Stop if not enough speed for next cast
			if (InOutSpeed < 0.f)
			{
				OutSceneCastInfo.EndLocation = SceneCastInfo.EndLocation;
				break;
			}

			// Stop if there was nothing to ricochet off of
			if (!RicochetableHit)
			{
				OutSceneCastInfo.EndLocation = SceneCastEnd;
				break;
			}
			// We have a ricochet hit

			if (DistanceTraveled == InDistanceCap)
			{
				// Edge case: we should end the whole thing if we ran out of distance exactly when we hit a ricochet
				OutSceneCastInfo.TotalDistanceTraveled = DistanceTraveled;
				OutSceneCastInfo.EndLocation = RicochetableHit->Location;
				break;
			}
		}


		// SETUP FOR OUR NEXT SCENE CAST
		// Next ricochet cast needs a new direction and start location
		CurrentSceneCastDirection = CurrentSceneCastDirection.MirrorByVector(RicochetableHit->ImpactNormal);
		CurrentSceneCastStart = RicochetableHit->Location + (CurrentSceneCastDirection * UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding);
	}

	OutSceneCastInfo.TotalDistanceTraveled = DistanceTraveled;
}
void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, TArray<FShooterHitResult>& OutShooterHits, FSceneCastInfo& OutSceneCastInfo, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, PerCmSpeedNerfStack, InWorld, OutShooterHits, OutSceneCastInfo, InStart, InDirection, InDistanceCap, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InRicochetCap, GetPenetrationSpeedNerf, GetRicochetSpeedNerf, IsHitRicochetable);
}
//  END Custom query

float UBFL_ShooterHelpers::NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm)
{
	const float SpeedToTakeAway = (InNerfPerCm * InDistanceToTravel);
	const float TraveledThroughRatio = (InOutSpeed / SpeedToTakeAway);
	const float TraveledThroughDistance = (TraveledThroughRatio * InDistanceToTravel);
	InOutSpeed -= SpeedToTakeAway;

	return TraveledThroughDistance;
}
