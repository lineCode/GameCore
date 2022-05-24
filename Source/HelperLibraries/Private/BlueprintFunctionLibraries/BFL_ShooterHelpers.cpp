// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "BlueprintFunctionLibraries/BFL_DrawDebugHelpers.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"



const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<bool(const FHitResult&)>& UBFL_ShooterHelpers::DefaultIsHitRicochetable = [](const FHitResult&) { return false; };

//  BEGIN Custom query
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FSceneCastResult& OutSceneCastResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	OutSceneCastResult.StartLocation = InStart;
	OutSceneCastResult.StartSpeed = InOutSpeed;
	OutSceneCastResult.StartDirection = (InEnd - InStart).GetSafeNormal();

	TArray<FExitAwareHitResult> HitResults;
	FExitAwareHitResult* ImpenetrableHit = UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(InWorld, HitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable, true);


	const FVector SceneCastDirection = (InEnd - InStart).GetSafeNormal();
	const float SceneCastDistance = HitResults.Num() > 0 ? UBFL_HitResultHelpers::CheapCalculateTraceLength(HitResults.Last()) : FVector::Distance(InStart, InEnd);

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
			OutSceneCastResult.EndLocation = InStart + (SceneCastDirection * TraveledThroughDistance);
			OutSceneCastResult.EndTime = TraveledThroughDistance / SceneCastDistance;
			OutSceneCastResult.LengthFromStartToEnd = TraveledThroughDistance;
			OutSceneCastResult.EndSpeed = 0.f;
			return nullptr;
		}
	}

	if (InOutSpeed > 0.f)
	{
		// Add found hit results to the OutShooterHits
		OutSceneCastResult.HitResults.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of speed
		for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
		{
			// Add this hit to our shooter hits
			FShooterHitResult& AddedShooterHit = OutSceneCastResult.HitResults.Add_GetRef(HitResults[i]);
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
				OutSceneCastResult.EndLocation = AddedShooterHit.Location;
				OutSceneCastResult.EndTime = AddedShooterHit.Time;
				OutSceneCastResult.LengthFromStartToEnd = AddedShooterHit.Distance;
				OutSceneCastResult.EndSpeed = InOutSpeed >= 0.f ? InOutSpeed : 0.f;
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
					OutSceneCastResult.EndLocation = AddedShooterHit.Location + (SceneCastDirection * TraveledThroughDistance);
					OutSceneCastResult.EndTime = AddedShooterHit.Time + (TraveledThroughDistance / SceneCastDistance);
					OutSceneCastResult.LengthFromStartToEnd = AddedShooterHit.Distance + TraveledThroughDistance;
					OutSceneCastResult.EndSpeed = 0.f;
					return nullptr;
				}
			}
		}
	}

	// InOutSpeed made it past every nerf
	OutSceneCastResult.EndLocation = InEnd;
	OutSceneCastResult.EndTime = 1.f;
	OutSceneCastResult.LengthFromStartToEnd = SceneCastDistance;
	OutSceneCastResult.EndSpeed = InOutSpeed;
	return nullptr;
}
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FSceneCastResult& OutSceneCastResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, PerCmSpeedNerfStack, InWorld, OutSceneCastResult, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
}
//  END Custom query

//  BEGIN Custom query
void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, TArray<FSceneCastResult>& OutSceneCastResults, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	FVector CurrentSceneCastStart = InStart;
	FVector CurrentSceneCastDirection = InDirection;
	float DistanceTraveled = 0.f;

	// The first iteration of this loop is the initial scene cast and the rest of the iterations are ricochet scene casts
	for (int32 RicochetNumber = 0; (RicochetNumber <= InRicochetCap || InRicochetCap == -1); ++RicochetNumber)
	{
		const FVector SceneCastEnd = CurrentSceneCastStart + (CurrentSceneCastDirection * (InDistanceCap - DistanceTraveled));

		FSceneCastResult& SceneCastResult = OutSceneCastResults.AddDefaulted_GetRef();
		FShooterHitResult* RicochetableHit = PenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, InOutPerCmSpeedNerfStack, InWorld, SceneCastResult, CurrentSceneCastStart, SceneCastEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, GetPenetrationSpeedNerf, IsHitRicochetable);
		DistanceTraveled += SceneCastResult.LengthFromStartToEnd;

		// Set more shooter hit result data
		{
			// Give data to our shooter hits for this scene cast
			for (FShooterHitResult& ShooterHit : SceneCastResult.HitResults)
			{
				ShooterHit.RicochetNumber = RicochetNumber;
				ShooterHit.TraveledDistanceBeforeThisTrace = (DistanceTraveled - SceneCastResult.LengthFromStartToEnd); // distance up until this scene cast
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

		// Check if we should end here
		{
			// Stop if not enough speed for next cast
			if (InOutSpeed < 0.f)
			{
				break;
			}

			// Stop if there was nothing to ricochet off of
			if (!RicochetableHit)
			{
				SceneCastResult.EndLocation = SceneCastEnd;
				break;
			}
			// We have a ricochet hit

			if (DistanceTraveled == InDistanceCap)
			{
				// Edge case: we should end the whole thing if we ran out of distance exactly when we hit a ricochet
				SceneCastResult.EndLocation = RicochetableHit->Location;
				break;
			}
		}


		// SETUP FOR OUR NEXT SCENE CAST
		// Next ricochet cast needs a new direction and start location
		CurrentSceneCastDirection = CurrentSceneCastDirection.MirrorByVector(RicochetableHit->ImpactNormal);
		CurrentSceneCastStart = RicochetableHit->Location + (CurrentSceneCastDirection * UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding);
	}
}
void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(float& InOutSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, TArray<FSceneCastResult>& OutSceneCastResults, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(InOutSpeed, PerCmSpeedNerfStack, InWorld, OutSceneCastResults, InStart, InDirection, InDistanceCap, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InRicochetCap, GetPenetrationSpeedNerf, GetRicochetSpeedNerf, IsHitRicochetable);
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

void UBFL_ShooterHelpers::DebugPenetrationSceneCastWithExitHitsUsingSpeed(const UWorld* InWorld, const FSceneCastResult& InSceneCastResult, const float InInitialSpeed, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor)
{
	const FVector Direction = (InSceneCastResult.EndLocation - InSceneCastResult.StartLocation).GetSafeNormal();
	const float SceneCastTravelDistance = InSceneCastResult.LengthFromStartToEnd;

	const float NumberOfLineSegments = FMath::CeilToInt(SceneCastTravelDistance / (InSegmentsLength + InSegmentsSpacingLength));
	for (int32 i = 0; i < NumberOfLineSegments; ++i)
	{
		float DistanceToLineSegmentStart = (InSegmentsLength + InSegmentsSpacingLength) * i;
		float DistanceToLineSegmentEnd = DistanceToLineSegmentStart + InSegmentsLength;
		if (DistanceToLineSegmentEnd > SceneCastTravelDistance)
		{
			DistanceToLineSegmentEnd = SceneCastTravelDistance;
		}

		const FVector LineSegmentStart = InSceneCastResult.StartLocation + (Direction * DistanceToLineSegmentStart);
		const FVector LineSegmentEnd = InSceneCastResult.StartLocation + (Direction * DistanceToLineSegmentEnd);


		// Get the speed at the line segment start
		float SpeedAtLineSegmentStart;
		{
			// NOTE: we use the term "Time" as in the ratio from FSceneCastResult::StartLocation to FSceneCastResult::EndLocation. This is not the same as FHitResult::Time!
			float TimeOnOrBeforeLineSegmentStart = 0.f;
			float SpeedOnOrBeforeLineSegmentStart = InSceneCastResult.StartSpeed;
			float TimeOnOrAfterLineSegmentStart = 1.f;
			float SpeedOnOrAfterLineSegmentStart = InSceneCastResult.EndSpeed;
			for (const FShooterHitResult& Hit : InSceneCastResult.HitResults)
			{
				if (Hit.Distance <= DistanceToLineSegmentStart)
				{
					// This hit is directly on or before the line segment start
					TimeOnOrBeforeLineSegmentStart = Hit.Time / InSceneCastResult.EndTime;
					SpeedOnOrBeforeLineSegmentStart = Hit.Speed;
					continue;
				}
				if (Hit.Distance >= DistanceToLineSegmentStart)
				{
					// This hit is directly on or after the line segment start
					TimeOnOrAfterLineSegmentStart = Hit.Time / InSceneCastResult.EndTime;
					SpeedOnOrAfterLineSegmentStart = Hit.Speed;
					break;
				}
			}

			const float TimeOfLineSegmentStart = DistanceToLineSegmentStart / SceneCastTravelDistance;
			SpeedAtLineSegmentStart = FMath::Lerp(SpeedOnOrBeforeLineSegmentStart, SpeedOnOrAfterLineSegmentStart, TimeOfLineSegmentStart / TimeOnOrAfterLineSegmentStart);
		}

		// Find the hits in between the line segment start and end
		TArray<FShooterHitResult> HitsWithinLineSegment;
		for (const FShooterHitResult& Hit : InSceneCastResult.HitResults)
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
			const FLinearColor SpeedColor = FMath::Lerp(FullSpeedColor, NoSpeedColor, 1 - (SpeedAtLineSegmentStart / InInitialSpeed));
			DrawDebugLine(InWorld, LineSegmentStart, LineSegmentEnd, SpeedColor.ToFColor(true), false, LifeTime, 0, Thickness);
		}
		else // there are hits (penetrations) within this segment so we will draw multible lines for this segment to give more accurate colors
		{
			// Debug line from the line segment start to the first hit
			{
				const FVector DebugLineStart = LineSegmentStart;
				const FVector DebugLineEnd = HitsWithinLineSegment[0].Location;
				const FLinearColor SpeedColor = FMath::Lerp(FullSpeedColor, NoSpeedColor, 1 - (SpeedAtLineSegmentStart / InInitialSpeed));
				DrawDebugLine(InWorld, DebugLineStart, DebugLineEnd, SpeedColor.ToFColor(true), false, LifeTime, 0, Thickness);
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
				const FLinearColor SpeedColor = FMath::Lerp(FullSpeedColor, NoSpeedColor, 1 - (HitsWithinLineSegment[j].Speed / InInitialSpeed));
				DrawDebugLine(InWorld, DebugLineStart, DebugLineEnd, SpeedColor.ToFColor(true), false, LifeTime, 0, Thickness);
			}

			// Debug line from the last hit to the line segment end
			{
				const FVector DebugLineStart = HitsWithinLineSegment.Last().Location;
				const FVector DebugLineEnd = LineSegmentEnd;
				const FLinearColor SpeedColor = FMath::Lerp(FullSpeedColor, NoSpeedColor, 1 - (HitsWithinLineSegment.Last().Speed / InInitialSpeed));
				DrawDebugLine(InWorld, HitsWithinLineSegment.Last().Location, LineSegmentEnd, SpeedColor.ToFColor(true), false, LifeTime, 0, Thickness);
			}
		}
	}
}

void UBFL_ShooterHelpers::DebugRicochetingPenetrationSceneCastWithExitHitsUsingSpeed(const UWorld* InWorld, const TArray<FSceneCastResult>& InSceneCastResults, const float InInitialSpeed, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor)
{
	for (const FSceneCastResult& SceneCastResult : InSceneCastResults)
	{
		DebugPenetrationSceneCastWithExitHitsUsingSpeed(InWorld, SceneCastResult, InInitialSpeed, bPersistentLines, LifeTime, DepthPriority, Thickness, InSegmentsLength, InSegmentsSpacingLength, FullSpeedColor, NoSpeedColor);
	}
}
