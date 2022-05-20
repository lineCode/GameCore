// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "DrawDebugHelpers.h"



const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<bool(const FHitResult&)>& UBFL_ShooterHelpers::DefaultIsHitRicochetable = [](const FHitResult&) { return false; };

FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	OutShootResult.StartLocation = InStart;
	OutShootResult.StartDirection = (InEnd - InStart).GetSafeNormal();

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
			OutShootResult.EndLocation = InStart + (SceneCastDirection * TraveledThroughDistance);
			OutShootResult.TotalDistanceTraveled = TraveledThroughDistance;
			return nullptr;
		}
	}

	if (InOutSpeed > 0.f)
	{
		// Add found hit results to the OutShootResult
		OutShootResult.ShooterHits.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of speed
		for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
		{
			// Add this hit to our shooter hits
			FShooterHitResult& AddedShooterHit = OutShootResult.ShooterHits.Add_GetRef(HitResults[i]);
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
				OutShootResult.EndLocation = AddedShooterHit.Location;
				OutShootResult.TotalDistanceTraveled = AddedShooterHit.Distance;
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
					OutShootResult.EndLocation = AddedShooterHit.Location + (SceneCastDirection * TraveledThroughDistance);
					OutShootResult.TotalDistanceTraveled = AddedShooterHit.Distance + TraveledThroughDistance;
					return nullptr;
				}
			}
		}
	}

	// InOutSpeed made it past every nerf
	OutShootResult.EndLocation = InEnd;
	OutShootResult.TotalDistanceTraveled = SceneCastDistance;
	return nullptr;
}
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, PerCmSpeedNerfStack, InWorld, OutShootResult, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
}

void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	OutShootResult.StartLocation = InStart;
	OutShootResult.StartDirection = InDirection;
	OutShootResult.InitialSpeed = InOutSpeed;

	FVector CurrentSceneCastStart = InStart;
	FVector CurrentSceneCastDirection = InDirection;
	float DistanceTraveled = 0.f;

	// The first iteration of this loop is the initial scene cast and the rest of the iterations are ricochet scene casts
	for (int32 RicochetNumber = 0; (RicochetNumber <= InRicochetCap || InRicochetCap == -1); ++RicochetNumber)
	{
		const FVector SceneCastEnd = CurrentSceneCastStart + (CurrentSceneCastDirection * (InDistanceCap - DistanceTraveled));

		FShootResult ShootResult;
		FShooterHitResult* RicochetableHit = PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, InOutPerCmSpeedNerfStack, InWorld, ShootResult, CurrentSceneCastStart, SceneCastEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitRicochetable);
		DistanceTraveled += ShootResult.TotalDistanceTraveled;

		// Set ShooterHit data
		{
			// Give data to our shooter hits for this scene cast
			for (FShooterHitResult& ShooterHit : ShootResult.ShooterHits)
			{
				ShooterHit.RicochetNumber = RicochetNumber;
				ShooterHit.TraveledDistanceBeforeThisTrace = (DistanceTraveled - ShootResult.TotalDistanceTraveled); // distance up until this scene cast
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


		// Finished with this cast. Add to our output
		OutShootResult.ShooterHits.Append(ShootResult.ShooterHits);

		// Check if we should end here
		{
			// Stop if not enough speed for next cast
			if (InOutSpeed < 0.f)
			{
				OutShootResult.EndLocation = ShootResult.EndLocation;
				break;
			}

			// Stop if there was nothing to ricochet off of
			if (!RicochetableHit)
			{
				OutShootResult.EndLocation = SceneCastEnd;
				break;
			}
			// We have a ricochet hit

			if (DistanceTraveled == InDistanceCap)
			{
				// Edge case: we should end the whole thing if we ran out of distance exactly when we hit a ricochet
				OutShootResult.TotalDistanceTraveled = DistanceTraveled;
				OutShootResult.EndLocation = RicochetableHit->Location;
				break;
			}
		}


		// SETUP FOR OUR NEXT SCENE CAST
		// Next ricochet cast needs a new direction and start location
		CurrentSceneCastDirection = CurrentSceneCastDirection.MirrorByVector(RicochetableHit->ImpactNormal);
		CurrentSceneCastStart = RicochetableHit->Location + (CurrentSceneCastDirection * UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding);
	}

	OutShootResult.TotalDistanceTraveled = DistanceTraveled;
}

void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FShootResult& OutShootResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, PerCmSpeedNerfStack, InWorld, OutShootResult, InStart, InDirection, InDistanceCap, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InRicochetCap, GetPenetrationSpeedNerf, GetRicochetSpeedNerf, IsHitRicochetable);
}

float UBFL_ShooterHelpers::NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm)
{
	const float SpeedToTakeAway = (InNerfPerCm * InDistanceToTravel);
	const float TraveledThroughRatio = (InOutSpeed / SpeedToTakeAway);
	const float TraveledThroughDistance = (TraveledThroughRatio * InDistanceToTravel);
	InOutSpeed -= SpeedToTakeAway;

	return TraveledThroughDistance;
}


void FShootResult::DebugShot(const UWorld* InWorld, const float InLineSegmentLength) const
{
#if ENABLE_DRAW_DEBUG
	const float DebugLifeTime = 5.f;
	const FColor TraceColor = FColor::Blue;
	const FColor HitColor = FColor::Red;
	const float Thickness = 1.f;


	int32 IndexToNextRicochet = ShooterHits.IndexOfByPredicate([](const FShooterHitResult& Hit) -> bool { return Hit.bIsRicochet; });
	FVector CurrentCastStart = StartLocation;
	FVector CurrentCastDirection = StartDirection;
	float CurrentTraveledDistanceBeforeThisCast = 0.f;
	const int32 NumberOfLineSegments = FMath::CeilToInt(TotalDistanceTraveled / InLineSegmentLength);
	for (int32 i = 0; i < NumberOfLineSegments; ++i)
	{
		const bool bIsGapSegment = (i % 2 != 0); // odd iterations will be gap segments
		FColor RandomColor = FLinearColor(FMath::RandRange(0.f, 1.f), FMath::RandRange(0.f, 1.f), FMath::RandRange(0.f, 1.f)).ToFColor(false);


		const float CurrentDistanceToSegmentStart = (InLineSegmentLength * i) - CurrentTraveledDistanceBeforeThisCast;
		const float CurrentDistanceToSegmentEnd = CurrentDistanceToSegmentStart + InLineSegmentLength;
		FVector SegmentStart = CurrentCastStart + (CurrentCastDirection * CurrentDistanceToSegmentStart);
		FVector SegmentEnd = CurrentCastStart + (CurrentCastDirection * CurrentDistanceToSegmentEnd);


		if (ShooterHits.IsValidIndex(IndexToNextRicochet))
		{
			const FShooterHitResult& RicochetHit = ShooterHits[IndexToNextRicochet];
			if (CurrentDistanceToSegmentEnd > RicochetHit.Distance)
			{
				if (!bIsGapSegment)
				{
					DrawDebugLine(InWorld, SegmentStart, RicochetHit.Location, RandomColor, false, DebugLifeTime, 0, Thickness);
				}

				if (ShooterHits.IsValidIndex(IndexToNextRicochet + 1))
				{
					const FShooterHitResult& HitAfterRicochet = ShooterHits[IndexToNextRicochet + 1];
					CurrentCastStart = HitAfterRicochet.TraceStart;
					CurrentCastDirection = (HitAfterRicochet.TraceEnd - HitAfterRicochet.TraceStart).GetSafeNormal();
					CurrentTraveledDistanceBeforeThisCast = HitAfterRicochet.TraveledDistanceBeforeThisTrace;
				}
				else
				{
					CurrentCastStart = RicochetHit.Location;
					CurrentCastDirection = (EndLocation - RicochetHit.Location).GetSafeNormal();
					CurrentTraveledDistanceBeforeThisCast = RicochetHit.GetTotalDistanceTraveledToThisHit();
				}

				SegmentEnd = CurrentCastStart + (CurrentCastDirection * CurrentDistanceToSegmentEnd);

				if (!bIsGapSegment)
				{
					DrawDebugLine(InWorld, RicochetHit.Location, SegmentEnd, RandomColor, false, DebugLifeTime, 0, Thickness);
				}

				int32 j = IndexToNextRicochet + 1;
				while (true)
				{
					if (j >= ShooterHits.Num())
					{
						IndexToNextRicochet = INDEX_NONE;
						break;
					}

					if (ShooterHits[j].bIsRicochet)
					{
						IndexToNextRicochet = j;
						break;
					}
				}

				continue;
			}
		}


		// Temporarily commented so we can focus on making above part always work
		/*if (!bIsGapSegment)
		{
			DrawDebugLine(InWorld, SegmentStart, SegmentEnd, RandomColor, false, DebugLifeTime, 0, Thickness);
		}*/



	}
#endif // ENABLE_DRAW_DEBUG
}
