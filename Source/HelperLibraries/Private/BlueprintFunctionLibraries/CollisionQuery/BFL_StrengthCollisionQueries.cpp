// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/CollisionQuery/BFL_StrengthCollisionQueries.h"

#include "BlueprintFunctionLibraries/CollisionQuery/BFL_CollisionQueries.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "BlueprintFunctionLibraries/BFL_DrawDebugHelpers.h"
#include "DrawDebugHelpers.h"



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


void FPenetrationSceneCastWithExitHitsUsingStrengthResult::DrawStrengthDebugLine(const UWorld* InWorld, const float InInitialStrength, const bool bInPersistentLines, const float InLifeTime, const uint8 InDepthPriority, const float InThickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor) const
{
#if ENABLE_DRAW_DEBUG
	const float SceneCastTravelDistance = StrengthSceneCastInfo.DistanceToStop;

	const float NumberOfLineSegments = FMath::CeilToInt(SceneCastTravelDistance / (InSegmentsLength + InSegmentsSpacingLength));
	for (int32 i = 0; i < NumberOfLineSegments; ++i)
	{
		float DistanceToLineSegmentStart = (InSegmentsLength + InSegmentsSpacingLength) * i;
		float DistanceToLineSegmentEnd = DistanceToLineSegmentStart + InSegmentsLength;
		if (DistanceToLineSegmentEnd > SceneCastTravelDistance)
		{
			DistanceToLineSegmentEnd = SceneCastTravelDistance;
		}

		const FVector LineSegmentStart = StrengthSceneCastInfo.StartLocation + (StrengthSceneCastInfo.CastDirection * DistanceToLineSegmentStart);
		const FVector LineSegmentEnd = StrengthSceneCastInfo.StartLocation + (StrengthSceneCastInfo.CastDirection * DistanceToLineSegmentEnd);


		// Get the strength at the line segment start
		float StrengthAtLineSegmentStart;
		{
			// NOTE: we use the term "Time" as in the ratio from FPenetrationSceneCastWithExitHitsUsingStrengthResult::StartLocation to FPenetrationSceneCastWithExitHitsUsingStrengthResult::StopLocation. This is not the same as FHitResult::Time!
			float TimeOnOrBeforeLineSegmentStart = 0.f;
			float StrengthOnOrBeforeLineSegmentStart = StrengthSceneCastInfo.StartStrength;
			float TimeOnOrAfterLineSegmentStart = 1.f;
			float StrengthOnOrAfterLineSegmentStart = StrengthSceneCastInfo.StopStrength;
			for (const FStrengthHitResult& Hit : HitResults)
			{
				if (Hit.Distance <= DistanceToLineSegmentStart)
				{
					// This hit is directly on or before the line segment start
					TimeOnOrBeforeLineSegmentStart = Hit.Time / StrengthSceneCastInfo.TimeAtStop;
					StrengthOnOrBeforeLineSegmentStart = Hit.Strength;
					continue;
				}
				if (Hit.Distance >= DistanceToLineSegmentStart)
				{
					// This hit is directly on or after the line segment start
					TimeOnOrAfterLineSegmentStart = Hit.Time / StrengthSceneCastInfo.TimeAtStop;
					StrengthOnOrAfterLineSegmentStart = Hit.Strength;
					break;
				}
			}

			const float TimeOfLineSegmentStart = DistanceToLineSegmentStart / SceneCastTravelDistance;
			StrengthAtLineSegmentStart = FMath::Lerp(StrengthOnOrBeforeLineSegmentStart, StrengthOnOrAfterLineSegmentStart, TimeOfLineSegmentStart / TimeOnOrAfterLineSegmentStart);
		}

		// Find the hits in between the line segment start and end
		TArray<FStrengthHitResult> HitsWithinLineSegment;
		for (const FStrengthHitResult& Hit : HitResults)
		{
			if (Hit.Distance <= DistanceToLineSegmentStart)
			{
				continue;
			}
			if (Hit.Distance >= DistanceToLineSegmentEnd)
			{
				break;
			}

			HitsWithinLineSegment.Add(Hit); // hit in between segment
		}


		// Draw the debug line(s)
		if (HitsWithinLineSegment.Num() <= 0)
		{
			const FColor StrengthDebugColor = GetDebugColorForStrength(StrengthAtLineSegmentStart, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);
			DrawDebugLine(InWorld, LineSegmentStart, LineSegmentEnd, StrengthDebugColor, false, InLifeTime, 0, InThickness);
		}
		else // there are hits (penetrations) within this segment so we will draw multible lines for this segment to give more accurate colors
		{
			// Debug line from the line segment start to the first hit
			{
				const FVector DebugLineStart = LineSegmentStart;
				const FVector DebugLineEnd = HitsWithinLineSegment[0].Location;
				const FColor StrengthDebugColor = GetDebugColorForStrength(StrengthAtLineSegmentStart, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);
				DrawDebugLine(InWorld, DebugLineStart, DebugLineEnd, StrengthDebugColor, false, InLifeTime, 0, InThickness);
			}

			// Debug lines from hit to hit
			for (int32 j = 0; j < HitsWithinLineSegment.Num(); ++j)
			{
				if (HitsWithinLineSegment.IsValidIndex(j + 1) == false)
				{
					break;
				}

				const FVector DebugLineStart = HitsWithinLineSegment[j].Location;
				const FVector DebugLineEnd = HitsWithinLineSegment[j + 1].Location;
				const FColor StrengthDebugColor = GetDebugColorForStrength(HitsWithinLineSegment[j].Strength, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);
				DrawDebugLine(InWorld, DebugLineStart, DebugLineEnd, StrengthDebugColor, false, InLifeTime, 0, InThickness);
			}

			// Debug line from the last hit to the line segment end
			{
				const FVector DebugLineStart = HitsWithinLineSegment.Last().Location;
				const FVector DebugLineEnd = LineSegmentEnd;
				const FColor StrengthDebugColor = GetDebugColorForStrength(HitsWithinLineSegment.Last().Strength, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);
				DrawDebugLine(InWorld, HitsWithinLineSegment.Last().Location, LineSegmentEnd, StrengthDebugColor, false, InLifeTime, 0, InThickness);
			}
		}
	}
#endif // ENABLE_DRAW_DEBUG
}
void FPenetrationSceneCastWithExitHitsUsingStrengthResult::DrawStrengthDebugText(const UWorld* InWorld, const float InInitialStrength, const float InLifeTime, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor) const
{
#if ENABLE_DRAW_DEBUG
	TArray<TPair<FVector, float>> LocationsWithStrengths;

	LocationsWithStrengths.Emplace(StrengthSceneCastInfo.StartLocation, StrengthSceneCastInfo.StartStrength);
	for (const FStrengthHitResult& Hit : HitResults)
	{
		LocationsWithStrengths.Emplace(Hit.Location, Hit.Strength);
	}
	LocationsWithStrengths.Emplace(StrengthSceneCastInfo.StopLocation, StrengthSceneCastInfo.StopStrength);


	const FVector OffsetDirection = FVector::UpVector;
	for (const TPair<FVector, float>& LocationWithStrength : LocationsWithStrengths)
	{
		const FColor StrengthDebugColor = GetDebugColorForStrength(LocationWithStrength.Value, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);

		const FVector StringLocation = LocationWithStrength.Key/* + (OffsetDirection * 10.f)*/;
		const FString DebugString = FString::Printf(TEXT("%.2f"), LocationWithStrength.Value);
		DrawDebugString(InWorld, StringLocation, DebugString, nullptr, StrengthDebugColor, InLifeTime);
	}
#endif // ENABLE_DRAW_DEBUG
}
void FPenetrationSceneCastWithExitHitsUsingStrengthResult::DrawCollisionShapeDebug(const UWorld* InWorld, const float InInitialStrength, const bool bInPersistentLines, const float InLifeTime, const uint8 InDepthPriority, const float InThickness, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor) const
{
#if ENABLE_DRAW_DEBUG
	TArray<TPair<FVector, float>> LocationsWithStrengths;

	LocationsWithStrengths.Emplace(StrengthSceneCastInfo.StartLocation, StrengthSceneCastInfo.StartStrength);
	for (const FStrengthHitResult& Hit : HitResults)
	{
		LocationsWithStrengths.Emplace(Hit.Location, Hit.Strength);
	}
	LocationsWithStrengths.Emplace(StrengthSceneCastInfo.StopLocation, StrengthSceneCastInfo.StopStrength);


	const FVector OffsetDirection = FVector::UpVector;
	for (const TPair<FVector, float>& LocationWithStrength : LocationsWithStrengths)
	{
		const FColor StrengthDebugColor = GetDebugColorForStrength(LocationWithStrength.Value, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);

		const FVector ShapeLocation = LocationWithStrength.Key;
		UBFL_DrawDebugHelpers::DrawDebugCollisionShape(InWorld, ShapeLocation, StrengthSceneCastInfo.CollisionShapeCasted, StrengthSceneCastInfo.CollisionShapeCastedRotation, StrengthDebugColor, 16, bInPersistentLines, InLifeTime, InDepthPriority, InThickness);
	}
#endif // ENABLE_DRAW_DEBUG
}
FLinearColor FPenetrationSceneCastWithExitHitsUsingStrengthResult::GetDebugColorForStrength(const float InStrength, const float InInitialStrength, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor)
{
	return FLinearColor::LerpUsingHSV(InFullStrengthColor, InNoStrengthColor, 1 - (InStrength / InInitialStrength));
}

void FRicochetingPenetrationSceneCastWithExitHitsUsingStrengthResult::DrawStrengthDebugLine(const UWorld* InWorld, const float InInitialStrength, const bool bInPersistentLines, const float InLifeTime, const uint8 InDepthPriority, const float InThickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor) const
{
#if ENABLE_DRAW_DEBUG
	for (const FPenetrationSceneCastWithExitHitsUsingStrengthResult& PenetrationSceneCastWithExitHitsUsingStrengthResult : PenetrationSceneCastWithExitHitsUsingStrengthResults)
	{
		PenetrationSceneCastWithExitHitsUsingStrengthResult.DrawStrengthDebugLine(InWorld, InInitialStrength, bInPersistentLines, InLifeTime, InDepthPriority, InThickness, InSegmentsLength, InSegmentsSpacingLength, InFullStrengthColor, InNoStrengthColor);
	}
#endif // ENABLE_DRAW_DEBUG
}
void FRicochetingPenetrationSceneCastWithExitHitsUsingStrengthResult::DrawStrengthDebugText(const UWorld* InWorld, const float InInitialStrength, const float InLifeTime, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor) const
{
#if ENABLE_DRAW_DEBUG
	FVector PreviousRicochetTextOffsetDirection = FVector::ZeroVector;
	for (int32 i = 0; i < PenetrationSceneCastWithExitHitsUsingStrengthResults.Num(); ++i)
	{
		const FPenetrationSceneCastWithExitHitsUsingStrengthResult& PenetrationSceneCastWithExitHitsUsingStrengthResult = PenetrationSceneCastWithExitHitsUsingStrengthResults[i];

		FVector RicochetTextOffsetDirection;

		// Collect locations and strengths
		TArray<TPair<FVector, float>> LocationsWithStrengths;
		LocationsWithStrengths.Emplace(PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StartLocation, PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StartStrength);
		for (const FStrengthHitResult& Hit : PenetrationSceneCastWithExitHitsUsingStrengthResult.HitResults)
		{
			LocationsWithStrengths.Emplace(Hit.Location, Hit.Strength);

			if (Hit.bIsRicochet)
			{
				// This will be the axis that we shift our debug string locations for the pre-ricochet and post-ricochet strengths
				RicochetTextOffsetDirection = FVector::CrossProduct(PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.CastDirection, Hit.ImpactNormal).GetSafeNormal();
			}
		}
		LocationsWithStrengths.Emplace(PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StopLocation, PenetrationSceneCastWithExitHitsUsingStrengthResult.StrengthSceneCastInfo.StopStrength);

		// Debug them
		const float RicochetTextOffsetAmount = 30.f;
		for (int j = 0; j < LocationsWithStrengths.Num(); ++j)
		{
			if (LocationsWithStrengths.IsValidIndex(j + 1) && LocationsWithStrengths[j] == LocationsWithStrengths[j + 1])
			{
				// Skip duplicates
				continue;
			}

			const FColor StrengthDebugColor = FPenetrationSceneCastWithExitHitsUsingStrengthResult::GetDebugColorForStrength(LocationsWithStrengths[j].Value, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);
			FVector StringLocation = LocationsWithStrengths[j].Key;

			if (j == LocationsWithStrengths.Num() - 1) // if we are the stop location
			{
				// If we are about to ricochet
				if (PenetrationSceneCastWithExitHitsUsingStrengthResult.HitResults.Num() > 0 && PenetrationSceneCastWithExitHitsUsingStrengthResult.HitResults.Last().bIsRicochet)
				{
					// Offset the pre-ricochet strength debug location upwards
					StringLocation += (-RicochetTextOffsetDirection * RicochetTextOffsetAmount);

					// If there is not a post-ricochet line after this
					if (i == PenetrationSceneCastWithExitHitsUsingStrengthResults.Num() - 1)
					{
						// Then just debug the post-ricochet strength here
						const FColor PostQueryStrengthDebugColor = FPenetrationSceneCastWithExitHitsUsingStrengthResult::GetDebugColorForStrength(StrengthSceneCastInfo.StopStrength, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);
						const FVector PostQueryStringLocation = StrengthSceneCastInfo.StopLocation + (RicochetTextOffsetDirection * RicochetTextOffsetAmount);
						const FString PostQueryDebugString = FString::Printf(TEXT("%.2f"), StrengthSceneCastInfo.StopStrength);
						DrawDebugString(InWorld, PostQueryStringLocation, PostQueryDebugString, nullptr, PostQueryStrengthDebugColor, InLifeTime);
					}
				}
			}
			else if (j == 0) // if we are the start location
			{
				// If we came from a ricochet
				if (PenetrationSceneCastWithExitHitsUsingStrengthResults.IsValidIndex(i - 1) && PenetrationSceneCastWithExitHitsUsingStrengthResults[i - 1].HitResults.Num() > 0 && PenetrationSceneCastWithExitHitsUsingStrengthResults[i - 1].HitResults.Last().bIsRicochet)
				{
					// Offset the post-ricochet strength debug location downwards
					StringLocation += (PreviousRicochetTextOffsetDirection * RicochetTextOffsetAmount);
				}
			}

			// Debug this location's strength
			const FString DebugString = FString::Printf(TEXT("%.2f"), LocationsWithStrengths[j].Value);
			DrawDebugString(InWorld, StringLocation, DebugString, nullptr, StrengthDebugColor, InLifeTime);
		}

		PreviousRicochetTextOffsetDirection = RicochetTextOffsetDirection;
	}
#endif // ENABLE_DRAW_DEBUG
}

void FRicochetingPenetrationSceneCastWithExitHitsUsingStrengthResult::DrawCollisionShapeDebug(const UWorld* InWorld, const float InInitialStrength, const bool bInPersistentLines, const float InLifeTime, const uint8 InDepthPriority, const float InThickness, const FLinearColor& InFullStrengthColor, const FLinearColor& InNoStrengthColor) const
{
#if ENABLE_DRAW_DEBUG
	for (const FPenetrationSceneCastWithExitHitsUsingStrengthResult& PenetrationSceneCastWithExitHitsUsingStrengthResult : PenetrationSceneCastWithExitHitsUsingStrengthResults)
	{
		PenetrationSceneCastWithExitHitsUsingStrengthResult.DrawCollisionShapeDebug(InWorld, InInitialStrength, false, InLifeTime, 0, 0, InFullStrengthColor, InNoStrengthColor);
	}

	// Draw collision shape debug for stop location on a ricochet. We are relying on the penetration casts to draw our stop locations, but if the query stops from a ricochet nerf, there won't be a next scene cast, so it's up to us to draw the end
	if (PenetrationSceneCastWithExitHitsUsingStrengthResults.Num() > 0)
	{
		if (StrengthSceneCastInfo.StopLocation == PenetrationSceneCastWithExitHitsUsingStrengthResults.Last().StrengthSceneCastInfo.StopLocation)
		{
			const FColor StrengthDebugColor = FPenetrationSceneCastWithExitHitsUsingStrengthResult::GetDebugColorForStrength(StrengthSceneCastInfo.StopStrength, InInitialStrength, InFullStrengthColor, InNoStrengthColor).ToFColor(true);

			const FVector ShapeDebugLocation = StrengthSceneCastInfo.StopLocation + (PenetrationSceneCastWithExitHitsUsingStrengthResults.Last().StrengthSceneCastInfo.CastDirection * UBFL_CollisionQueries::SceneCastStartWallAvoidancePadding);
			UBFL_DrawDebugHelpers::DrawDebugCollisionShape(InWorld, ShapeDebugLocation, StrengthSceneCastInfo.CollisionShapeCasted, StrengthSceneCastInfo.CollisionShapeCastedRotation, StrengthDebugColor, 16, bInPersistentLines, InLifeTime, InDepthPriority, InThickness);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}
