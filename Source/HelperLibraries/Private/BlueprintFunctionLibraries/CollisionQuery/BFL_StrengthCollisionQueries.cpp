// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/CollisionQuery/BFL_StrengthCollisionQueries.h"

#include "BlueprintFunctionLibraries/CollisionQuery/BFL_CollisionQueries.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"



const TFunctionRef<float(const FHitResult&)>& UBFL_StrengthCollisionQueries::DefaultGetPenetrationStrengthNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<float(const FHitResult&)>& UBFL_StrengthCollisionQueries::DefaultGetRicochetStrengthNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<bool(const FHitResult&)>& UBFL_StrengthCollisionQueries::DefaultIsHitRicochetable = [](const FHitResult&) { return false; };

//  BEGIN Custom query
FStrengthHitResult* UBFL_StrengthCollisionQueries::PenetrationSceneCastWithExitHitsUsingStrength(const float InInitialStrength, TArray<float>& InOutPerCmStrengthNerfStack, const UWorld* InWorld, FPenetrationSceneCastWithExitHitsUsingStrengthResult& OutResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationStrengthNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	OutResult.StrengthSceneCastInfo.CollisionShapeCasted = InCollisionShape;
	OutResult.StrengthSceneCastInfo.CollisionShapeCastedRotation = InRotation;
	OutResult.StrengthSceneCastInfo.StartLocation = InStart;
	OutResult.StrengthSceneCastInfo.StartStrength = InInitialStrength;
	OutResult.StrengthSceneCastInfo.CastDirection = (InEnd - InStart).GetSafeNormal();

	TArray<FExitAwareHitResult> HitResults;
	FExitAwareHitResult* ImpenetrableHit = UBFL_CollisionQueries::PenetrationSceneCastWithExitHits(InWorld, HitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, IsHitImpenetrable, true);


	const FVector SceneCastDirection = (InEnd - InStart).GetSafeNormal();
	const float SceneCastDistance = HitResults.Num() > 0 ? UBFL_HitResultHelpers::CheapCalculateTraceLength(HitResults.Last()) : FVector::Distance(InStart, InEnd);

	float CurrentStrength = InInitialStrength;

	// For this segment (from TraceStart to the first hit), apply strength nerfs and see if we ran out
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

		// Calculate how much strength per cm we should be taking away for this segment
		// Accumulate all of the strength nerfs from the InOutPerCmStrengthNerfStack
		float StrengthToTakeAwayPerCm = 0.f;
		for (const float& StrengthNerf : InOutPerCmStrengthNerfStack)
		{
			StrengthToTakeAwayPerCm += StrengthNerf;
		}

		// If we ran out of strength in this segment, stop adding further hits and return the stop location
		const float TraveledThroughDistance = NerfStrengthPerCm(CurrentStrength, SegmentDistance, StrengthToTakeAwayPerCm);
		if (CurrentStrength < 0.f)
		{
			OutResult.StrengthSceneCastInfo.StopLocation = InStart + (SceneCastDirection * TraveledThroughDistance);
			OutResult.StrengthSceneCastInfo.TimeAtStop = TraveledThroughDistance / SceneCastDistance;
			OutResult.StrengthSceneCastInfo.DistanceToStop = TraveledThroughDistance;
			OutResult.StrengthSceneCastInfo.StopStrength = 0.f;
			return nullptr;
		}
	}

	if (CurrentStrength > 0.f)
	{
		// Add found hit results to the OutStrengthHits
		OutResult.HitResults.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of strength
		for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
		{
			// Add this hit to our Strength hits
			FStrengthHitResult& AddedStrengthHit = OutResult.HitResults.Add_GetRef(HitResults[i]);
			AddedStrengthHit.Strength = CurrentStrength;

			if (HitResults[i].bStartPenetrating)
			{
				// Initial overlaps would mess up our PerCmStrengthNerfStack so skip it
				// Btw this is only a thing for simple collision queries
				UE_LOG(LogStrengthCollisionQueries, Verbose, TEXT("%s() Penetration strength query started inside of something. Make sure to not start this query inside of geometry. We will not consider this hit for the penetration strength nerf stack but it will still be included in the outputed hits. Hit Actor: [%s]."), ANSI_TO_TCHAR(__FUNCTION__), GetData(HitResults[i].GetActor()->GetName()));
				continue;
			}

			if (ImpenetrableHit && &HitResults[i] == ImpenetrableHit)
			{
				// Stop - don't calculate penetration nerfing on impenetrable hit
				OutResult.StrengthSceneCastInfo.StopLocation = AddedStrengthHit.Location;
				OutResult.StrengthSceneCastInfo.TimeAtStop = AddedStrengthHit.Time;
				OutResult.StrengthSceneCastInfo.DistanceToStop = AddedStrengthHit.Distance;
				OutResult.StrengthSceneCastInfo.StopStrength = FMath::Max(CurrentStrength, 0.f);
				return &AddedStrengthHit;
			}

			// Update the InOutPerCmStrengthNerfStack with this hit
			if (AddedStrengthHit.bIsExitHit == false)		// Add new nerf if we are entering something
			{
				InOutPerCmStrengthNerfStack.Push(GetPenetrationStrengthNerf(AddedStrengthHit));
			}
			else										// Remove most recent nerf if we are exiting something
			{
				const int32 IndexOfNerfThatWeAreExiting = InOutPerCmStrengthNerfStack.FindLast(GetPenetrationStrengthNerf(AddedStrengthHit));

				if (IndexOfNerfThatWeAreExiting != INDEX_NONE)
				{
					InOutPerCmStrengthNerfStack.RemoveAt(IndexOfNerfThatWeAreExiting);
				}
				else
				{
					UE_LOG(LogStrengthCollisionQueries, Error, TEXT("%s() Exited a penetration nerf that was never entered. This must be the callers fault by his GetPenetrationStrengthNerf() not having consistent strength nerfs for entrances and exits. Hit Actor: [%s]."), ANSI_TO_TCHAR(__FUNCTION__), GetData(AddedStrengthHit.GetActor()->GetName()));
				}
			}

			// For this segment (from this hit to the next hit), apply strength nerfs and see if we ran out
			{
				// Calculate this segment's distance
				float SegmentDistance;
				if (HitResults.IsValidIndex(i + 1))
				{
					// Get distance from this hit to the next hit
					SegmentDistance = (HitResults[i + 1].Distance - AddedStrengthHit.Distance); // distance from [i] to [i + 1]
				}
				else
				{
					// Get distance from this hit to trace end
					SegmentDistance = (SceneCastDistance - AddedStrengthHit.Distance);
				}

				// Calculate how much strength per cm we should be taking away for this segment
				// Accumulate all of the strength nerfs from the InOutPerCmStrengthNerfStack
				float StrengthToTakeAwayPerCm = 0.f;
				for (const float& StrengthNerf : InOutPerCmStrengthNerfStack)
				{
					StrengthToTakeAwayPerCm += StrengthNerf;
				}

				// If we ran out of strength in this segment, stop adding further hits and return the stop location
				const float TraveledThroughDistance = NerfStrengthPerCm(CurrentStrength, SegmentDistance, StrengthToTakeAwayPerCm);
				if (CurrentStrength < 0.f)
				{
					OutResult.StrengthSceneCastInfo.StopLocation = AddedStrengthHit.Location + (SceneCastDirection * TraveledThroughDistance);
					OutResult.StrengthSceneCastInfo.TimeAtStop = AddedStrengthHit.Time + (TraveledThroughDistance / SceneCastDistance);
					OutResult.StrengthSceneCastInfo.DistanceToStop = AddedStrengthHit.Distance + TraveledThroughDistance;
					OutResult.StrengthSceneCastInfo.StopStrength = 0.f;
					return nullptr;
				}
			}
		}
	}

	// CurrentStrength made it past every nerf
	OutResult.StrengthSceneCastInfo.StopLocation = InEnd;
	OutResult.StrengthSceneCastInfo.TimeAtStop = 1.f;
	OutResult.StrengthSceneCastInfo.DistanceToStop = SceneCastDistance;
	OutResult.StrengthSceneCastInfo.StopStrength = CurrentStrength;
	return nullptr;
}
FStrengthHitResult* UBFL_StrengthCollisionQueries::PenetrationSceneCastWithExitHitsUsingStrength(const float InInitialStrength, const float InRangeFalloffNerf, const UWorld* InWorld, FPenetrationSceneCastWithExitHitsUsingStrengthResult& OutResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationStrengthNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> PerCmStrengthNerfStack;
	PerCmStrengthNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingStrength(InInitialStrength, PerCmStrengthNerfStack, InWorld, OutResult, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, GetPenetrationStrengthNerf, IsHitImpenetrable);
}
//  END Custom query

//  BEGIN Custom query
void UBFL_StrengthCollisionQueries::RicochetingPenetrationSceneCastWithExitHitsUsingStrength(const float InInitialStrength, TArray<float>& InOutPerCmStrengthNerfStack, const UWorld* InWorld, FRicochetingPenetrationSceneCastWithExitHitsUsingStrengthResult& OutResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationStrengthNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetStrengthNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	if (InDistanceCap <= 0.f)
	{
		check(0);
		return;
	}

	OutResult.StrengthSceneCastInfo.CollisionShapeCasted = InCollisionShape;
	OutResult.StrengthSceneCastInfo.CollisionShapeCastedRotation = InRotation;
	OutResult.StrengthSceneCastInfo.StartLocation = InStart;
	OutResult.StrengthSceneCastInfo.StartStrength = InInitialStrength;
	OutResult.StrengthSceneCastInfo.CastDirection = InDirection;

	FVector CurrentSceneCastStart = InStart;
	FVector CurrentSceneCastDirection = InDirection;
	float DistanceTraveled = 0.f;
	float CurrentStrength = InInitialStrength;

	// The first iteration of this loop is the initial scene cast and the rest of the iterations are ricochet scene casts
	for (int32 RicochetNumber = 0; (RicochetNumber <= InRicochetCap || InRicochetCap == -1); ++RicochetNumber)
	{
		const FVector SceneCastEnd = CurrentSceneCastStart + (CurrentSceneCastDirection * (InDistanceCap - DistanceTraveled));

		FPenetrationSceneCastWithExitHitsUsingStrengthResult& PenetrationSceneCastWithExitHitsUsingStrengthResult = OutResult.PenetrationSceneCastWithExitHitsUsingStrengthResults.AddDefaulted_GetRef();
		FStrengthHitResult* RicochetableHit = PenetrationSceneCastWithExitHitsUsingStrength(CurrentStrength, InOutPerCmStrengthNerfStack, InWorld, PenetrationSceneCastWithExitHitsUsingStrengthResult, CurrentSceneCastStart, SceneCastEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, GetPenetrationStrengthNerf, IsHitRicochetable);

		DistanceTraveled += PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.DistanceToStop;
		CurrentStrength = PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StopStrength;

		// Set more strength hit result data
		{
			// Give data to our strength hits for this scene cast
			for (FStrengthHitResult& StrengthHit : PenetrationSceneCastWithExitHitsUsingStrengthResult.HitResults)
			{
				StrengthHit.RicochetNumber = RicochetNumber;
				StrengthHit.TraveledDistanceBeforeThisTrace = (DistanceTraveled - PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.DistanceToStop); // distance up until this scene cast
			}

			// Give data to the ricochet hit
			if (RicochetableHit)
			{
				RicochetableHit->bIsRicochet = true;
			}
		}

		// Apply ricochet strength nerf
		if (RicochetableHit)
		{
			CurrentStrength -= GetRicochetStrengthNerf(*RicochetableHit);
		}

		// Check if we should end here
		{
			// Stop if not enough strength for next cast
			if (CurrentStrength <= 0.f)
			{
				CurrentStrength = 0.f;
				break;
			}

			// Stop if there was nothing to ricochet off of
			if (!RicochetableHit)
			{
				PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StopLocation = SceneCastEnd;
				break;
			}
			// We have a ricochet hit

			if (DistanceTraveled == InDistanceCap)
			{
				// Edge case: we should end the whole thing if we ran out of distance exactly when we hit a ricochet
				PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StopLocation = RicochetableHit->Location;
				break;
			}
		}


		// SETUP FOR OUR NEXT SCENE CAST
		// Next ricochet cast needs a new direction and start location
		CurrentSceneCastDirection = CurrentSceneCastDirection.MirrorByVector(RicochetableHit->ImpactNormal);
		CurrentSceneCastStart = RicochetableHit->Location + (CurrentSceneCastDirection * UBFL_CollisionQueries::SceneCastStartWallAvoidancePadding);
	}

	OutResult.StrengthSceneCastInfo.StopStrength = CurrentStrength;
	OutResult.StrengthSceneCastInfo.DistanceToStop = DistanceTraveled;
	OutResult.StrengthSceneCastInfo.TimeAtStop = DistanceTraveled / InDistanceCap;

	if (OutResult.PenetrationSceneCastWithExitHitsUsingStrengthResults.Num() > 0)
	{
		OutResult.StrengthSceneCastInfo.StopLocation = OutResult.PenetrationSceneCastWithExitHitsUsingStrengthResults.Last().StrengthSceneCastInfo.StopLocation;
	}
	else
	{
		OutResult.StrengthSceneCastInfo.StopLocation = OutResult.StrengthSceneCastInfo.StartLocation;
	}
}
void UBFL_StrengthCollisionQueries::RicochetingPenetrationSceneCastWithExitHitsUsingStrength(const float InInitialStrength, const float InRangeFalloffNerf, const UWorld* InWorld, FRicochetingPenetrationSceneCastWithExitHitsUsingStrengthResult& OutResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationStrengthNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetStrengthNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	TArray<float> PerCmStrengthNerfStack;
	PerCmStrengthNerfStack.Push(InRangeFalloffNerf);
	return RicochetingPenetrationSceneCastWithExitHitsUsingStrength(InInitialStrength, PerCmStrengthNerfStack, InWorld, OutResult, InStart, InDirection, InDistanceCap, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, InRicochetCap, GetPenetrationStrengthNerf, GetRicochetStrengthNerf, IsHitRicochetable);
}
//  END Custom query

float UBFL_StrengthCollisionQueries::NerfStrengthPerCm(float& InOutStrength, const float InDistanceToTravel, const float InNerfPerCm)
{
	const float StrengthToTakeAway = (InNerfPerCm * InDistanceToTravel);
	const float TraveledThroughRatio = (InOutStrength / StrengthToTakeAway);
	const float TraveledThroughDistance = (TraveledThroughRatio * InDistanceToTravel);
	InOutStrength -= StrengthToTakeAway;

	return TraveledThroughDistance;
}
