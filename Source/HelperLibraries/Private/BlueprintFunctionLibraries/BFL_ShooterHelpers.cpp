// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"
#include "BlueprintFunctionLibraries/BFL_DrawDebugHelpers.h"
#include "DrawDebugHelpers.h"



const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetPenetrationSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<float(const FHitResult&)>& UBFL_ShooterHelpers::DefaultGetRicochetSpeedNerf = [](const FHitResult&) { return 0.f; };
const TFunctionRef<bool(const FHitResult&)>& UBFL_ShooterHelpers::DefaultIsHitRicochetable = [](const FHitResult&) { return false; };

//  BEGIN Custom query
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	OutResult.StartLocation = InStart;
	OutResult.StartSpeed = InInitialSpeed;
	OutResult.CastDirection = (InEnd - InStart).GetSafeNormal();

	TArray<FExitAwareHitResult> HitResults;
	FExitAwareHitResult* ImpenetrableHit = UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(InWorld, HitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, IsHitImpenetrable, true);


	const FVector SceneCastDirection = (InEnd - InStart).GetSafeNormal();
	const float SceneCastDistance = HitResults.Num() > 0 ? UBFL_HitResultHelpers::CheapCalculateTraceLength(HitResults.Last()) : FVector::Distance(InStart, InEnd);

	float CurrentSpeed = InInitialSpeed;

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
		const float TraveledThroughDistance = NerfSpeedPerCm(CurrentSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
		if (CurrentSpeed < 0.f)
		{
			OutResult.StopLocation = InStart + (SceneCastDirection * TraveledThroughDistance);
			OutResult.TimeAtStop = TraveledThroughDistance / SceneCastDistance;
			OutResult.DistanceToStop = TraveledThroughDistance;
			OutResult.StopSpeed = 0.f;
			return nullptr;
		}
	}

	if (CurrentSpeed > 0.f)
	{
		// Add found hit results to the OutShooterHits
		OutResult.HitResults.Reserve(HitResults.Num()); // assume that we will add all of the hits. But, there may end up being reserved space that goes unused if we run out of speed
		for (int32 i = 0; i < HitResults.Num(); ++i) // loop through all hits, comparing each hit with the next so we can treat them as semgents
		{
			// Add this hit to our shooter hits
			FShooterHitResult& AddedShooterHit = OutResult.HitResults.Add_GetRef(HitResults[i]);
			AddedShooterHit.Speed = CurrentSpeed;

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
				OutResult.StopLocation = AddedShooterHit.Location;
				OutResult.TimeAtStop = AddedShooterHit.Time;
				OutResult.DistanceToStop = AddedShooterHit.Distance;
				OutResult.StopSpeed = FMath::Max(CurrentSpeed, 0.f);
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
				const float TraveledThroughDistance = NerfSpeedPerCm(CurrentSpeed, SegmentDistance, SpeedToTakeAwayPerCm);
				if (CurrentSpeed < 0.f)
				{
					OutResult.StopLocation = AddedShooterHit.Location + (SceneCastDirection * TraveledThroughDistance);
					OutResult.TimeAtStop = AddedShooterHit.Time + (TraveledThroughDistance / SceneCastDistance);
					OutResult.DistanceToStop = AddedShooterHit.Distance + TraveledThroughDistance;
					OutResult.StopSpeed = 0.f;
					return nullptr;
				}
			}
		}
	}

	// CurrentSpeed made it past every nerf
	OutResult.StopLocation = InEnd;
	OutResult.TimeAtStop = 1.f;
	OutResult.DistanceToStop = SceneCastDistance;
	OutResult.StopSpeed = CurrentSpeed;
	return nullptr;
}
FShooterHitResult* UBFL_ShooterHelpers::PenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return PenetrationSceneCastWithExitHitsUsingSpeed(InInitialSpeed, PerCmSpeedNerfStack, InWorld, OutResult, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, GetPenetrationSpeedNerf, IsHitImpenetrable);
}
//  END Custom query

//  BEGIN Custom query
void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, TArray<float>& InOutPerCmSpeedNerfStack, const UWorld* InWorld, FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	FVector CurrentSceneCastStart = InStart;
	FVector CurrentSceneCastDirection = InDirection;
	float DistanceTraveled = 0.f;
	float CurrentSpeed = InInitialSpeed;

	// The first iteration of this loop is the initial scene cast and the rest of the iterations are ricochet scene casts
	for (int32 RicochetNumber = 0; (RicochetNumber <= InRicochetCap || InRicochetCap == -1); ++RicochetNumber)
	{
		const FVector SceneCastEnd = CurrentSceneCastStart + (CurrentSceneCastDirection * (InDistanceCap - DistanceTraveled));

		FPenetrationSceneCastWithExitHitsUsingSpeedResult& PenetrationSceneCastWithExitHitsUsingSpeedResult = OutResult.PenetrationSceneCastWithExitHitsUsingSpeedResults.AddDefaulted_GetRef();
		FShooterHitResult* RicochetableHit = PenetrationSceneCastWithExitHitsUsingSpeed(CurrentSpeed, InOutPerCmSpeedNerfStack, InWorld, PenetrationSceneCastWithExitHitsUsingSpeedResult, CurrentSceneCastStart, SceneCastEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, GetPenetrationSpeedNerf, IsHitRicochetable);

		DistanceTraveled += PenetrationSceneCastWithExitHitsUsingSpeedResult.DistanceToStop;
		CurrentSpeed = PenetrationSceneCastWithExitHitsUsingSpeedResult.StopSpeed;

		// Set more shooter hit result data
		{
			// Give data to our shooter hits for this scene cast
			for (FShooterHitResult& ShooterHit : PenetrationSceneCastWithExitHitsUsingSpeedResult.HitResults)
			{
				ShooterHit.RicochetNumber = RicochetNumber;
				ShooterHit.TraveledDistanceBeforeThisTrace = (DistanceTraveled - PenetrationSceneCastWithExitHitsUsingSpeedResult.DistanceToStop); // distance up until this scene cast
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
			CurrentSpeed -= GetRicochetSpeedNerf(*RicochetableHit);
		}

		// Check if we should end here
		{
			// Stop if not enough speed for next cast
			if (CurrentSpeed <= 0.f)
			{
				CurrentSpeed = 0.f;
				break;
			}

			// Stop if there was nothing to ricochet off of
			if (!RicochetableHit)
			{
				PenetrationSceneCastWithExitHitsUsingSpeedResult.StopLocation = SceneCastEnd;
				break;
			}
			// We have a ricochet hit

			if (DistanceTraveled == InDistanceCap)
			{
				// Edge case: we should end the whole thing if we ran out of distance exactly when we hit a ricochet
				PenetrationSceneCastWithExitHitsUsingSpeedResult.StopLocation = RicochetableHit->Location;
				break;
			}
		}


		// SETUP FOR OUR NEXT SCENE CAST
		// Next ricochet cast needs a new direction and start location
		CurrentSceneCastDirection = CurrentSceneCastDirection.MirrorByVector(RicochetableHit->ImpactNormal);
		CurrentSceneCastStart = RicochetableHit->Location + (CurrentSceneCastDirection * UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding);
	}

	OutResult.EndSpeed = CurrentSpeed;
	OutResult.DistanceTraveled = DistanceTraveled;
}
void UBFL_ShooterHelpers::RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(const float InInitialSpeed, const float InRangeFalloffNerf, const UWorld* InWorld, FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult& OutResult, const FVector& InStart, const FVector& InDirection, const float InDistanceCap, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const int32 InRicochetCap,
	const TFunctionRef<float(const FHitResult&)>& GetPenetrationSpeedNerf,
	const TFunctionRef<float(const FHitResult&)>& GetRicochetSpeedNerf,
	const TFunctionRef<bool(const FHitResult&)>& IsHitRicochetable)
{
	TArray<float> PerCmSpeedNerfStack;
	PerCmSpeedNerfStack.Push(InRangeFalloffNerf);
	return RicochetingPenetrationSceneCastWithExitHitsUsingSpeed(InInitialSpeed, PerCmSpeedNerfStack, InWorld, OutResult, InStart, InDirection, InDistanceCap, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, InRicochetCap, GetPenetrationSpeedNerf, GetRicochetSpeedNerf, IsHitRicochetable);
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


void FPenetrationSceneCastWithExitHitsUsingSpeedResult::DrawDebugLine(const UWorld* InWorld, const float InInitialSpeed, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor) const
{
	const float SceneCastTravelDistance = DistanceToStop;

	const float NumberOfLineSegments = FMath::CeilToInt(SceneCastTravelDistance / (InSegmentsLength + InSegmentsSpacingLength));
	for (int32 i = 0; i < NumberOfLineSegments; ++i)
	{
		float DistanceToLineSegmentStart = (InSegmentsLength + InSegmentsSpacingLength) * i;
		float DistanceToLineSegmentEnd = DistanceToLineSegmentStart + InSegmentsLength;
		if (DistanceToLineSegmentEnd > SceneCastTravelDistance)
		{
			DistanceToLineSegmentEnd = SceneCastTravelDistance;
		}

		const FVector LineSegmentStart = StartLocation + (CastDirection * DistanceToLineSegmentStart);
		const FVector LineSegmentEnd = StartLocation + (CastDirection * DistanceToLineSegmentEnd);


		// Get the speed at the line segment start
		float SpeedAtLineSegmentStart;
		{
			// NOTE: we use the term "Time" as in the ratio from FPenetrationSceneCastWithExitHitsUsingSpeedResult::StartLocation to FPenetrationSceneCastWithExitHitsUsingSpeedResult::StopLocation. This is not the same as FHitResult::Time!
			float TimeOnOrBeforeLineSegmentStart = 0.f;
			float SpeedOnOrBeforeLineSegmentStart = StartSpeed;
			float TimeOnOrAfterLineSegmentStart = 1.f;
			float SpeedOnOrAfterLineSegmentStart = StopSpeed;
			for (const FShooterHitResult& Hit : HitResults)
			{
				if (Hit.Distance <= DistanceToLineSegmentStart)
				{
					// This hit is directly on or before the line segment start
					TimeOnOrBeforeLineSegmentStart = Hit.Time / TimeAtStop;
					SpeedOnOrBeforeLineSegmentStart = Hit.Speed;
					continue;
				}
				if (Hit.Distance >= DistanceToLineSegmentStart)
				{
					// This hit is directly on or after the line segment start
					TimeOnOrAfterLineSegmentStart = Hit.Time / TimeAtStop;
					SpeedOnOrAfterLineSegmentStart = Hit.Speed;
					break;
				}
			}

			const float TimeOfLineSegmentStart = DistanceToLineSegmentStart / SceneCastTravelDistance;
			SpeedAtLineSegmentStart = FMath::Lerp(SpeedOnOrBeforeLineSegmentStart, SpeedOnOrAfterLineSegmentStart, TimeOfLineSegmentStart / TimeOnOrAfterLineSegmentStart);
		}

		// Find the hits in between the line segment start and end
		TArray<FShooterHitResult> HitsWithinLineSegment;
		for (const FShooterHitResult& Hit : HitResults)
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
			const FColor SpeedDebugColor = GetDebugColorForSpeed(SpeedAtLineSegmentStart, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);
			::DrawDebugLine(InWorld, LineSegmentStart, LineSegmentEnd, SpeedDebugColor, false, LifeTime, 0, Thickness);
		}
		else // there are hits (penetrations) within this segment so we will draw multible lines for this segment to give more accurate colors
		{
			// Debug line from the line segment start to the first hit
			{
				const FVector DebugLineStart = LineSegmentStart;
				const FVector DebugLineEnd = HitsWithinLineSegment[0].Location;
				const FColor SpeedDebugColor = GetDebugColorForSpeed(SpeedAtLineSegmentStart, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);
				::DrawDebugLine(InWorld, DebugLineStart, DebugLineEnd, SpeedDebugColor, false, LifeTime, 0, Thickness);
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
				const FColor SpeedDebugColor = GetDebugColorForSpeed(HitsWithinLineSegment[j].Speed, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);
				::DrawDebugLine(InWorld, DebugLineStart, DebugLineEnd, SpeedDebugColor, false, LifeTime, 0, Thickness);
			}

			// Debug line from the last hit to the line segment end
			{
				const FVector DebugLineStart = HitsWithinLineSegment.Last().Location;
				const FVector DebugLineEnd = LineSegmentEnd;
				const FColor SpeedDebugColor = GetDebugColorForSpeed(HitsWithinLineSegment.Last().Speed, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);
				::DrawDebugLine(InWorld, HitsWithinLineSegment.Last().Location, LineSegmentEnd, SpeedDebugColor, false, LifeTime, 0, Thickness);
			}
		}
	}
}
void FPenetrationSceneCastWithExitHitsUsingSpeedResult::DrawDebugText(const UWorld* InWorld, const float InInitialSpeed, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor) const
{
	TArray<TPair<FVector, float>> LocationsWithSpeeds;

	LocationsWithSpeeds.Emplace(StartLocation, StartSpeed);
	for (const FShooterHitResult& Hit : HitResults)
	{
		LocationsWithSpeeds.Emplace(Hit.Location, Hit.Speed);
	}
	LocationsWithSpeeds.Emplace(StopLocation, StopSpeed);


	const FVector OffsetDirection = FVector::UpVector;
	for (const TPair<FVector, float>& LocationWithSpeed : LocationsWithSpeeds)
	{
		const FColor SpeedDebugColor = GetDebugColorForSpeed(LocationWithSpeed.Value, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);

		const FVector StringLocation = LocationWithSpeed.Key/* + (OffsetDirection * 10.f)*/;
		const FString DebugString = FString::Printf(TEXT("%.2f"), LocationWithSpeed.Value);
		::DrawDebugString(InWorld, StringLocation, DebugString, nullptr, SpeedDebugColor, LifeTime);
	}
}

FLinearColor FPenetrationSceneCastWithExitHitsUsingSpeedResult::GetDebugColorForSpeed(const float InSpeed, const float InInitialSpeed, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor)
{
	return FLinearColor::LerpUsingHSV(FullSpeedColor, NoSpeedColor, 1 - (InSpeed / InInitialSpeed));
}

void FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult::Debug(const UWorld* InWorld, const float InInitialSpeed, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor) const
{
	for (const FPenetrationSceneCastWithExitHitsUsingSpeedResult& PenetrationSceneCastWithExitHitsUsingSpeedResult : PenetrationSceneCastWithExitHitsUsingSpeedResults)
	{
		PenetrationSceneCastWithExitHitsUsingSpeedResult.DrawDebugLine(InWorld, InInitialSpeed, bPersistentLines, LifeTime, DepthPriority, Thickness, InSegmentsLength, InSegmentsSpacingLength, FullSpeedColor, NoSpeedColor);
	}

	DrawDebugText(InWorld, InInitialSpeed, bPersistentLines, LifeTime, DepthPriority, Thickness, InSegmentsLength, InSegmentsSpacingLength, FullSpeedColor, NoSpeedColor);
}
void FRicochetingPenetrationSceneCastWithExitHitsUsingSpeedResult::DrawDebugText(const UWorld* InWorld, const float InInitialSpeed, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness, const float InSegmentsLength, const float InSegmentsSpacingLength, const FLinearColor& FullSpeedColor, const FLinearColor& NoSpeedColor) const
{
	FVector PreviousRicochetTextOffsetDirection = FVector::ZeroVector;
	for (int32 i = 0; i < PenetrationSceneCastWithExitHitsUsingSpeedResults.Num(); ++i)
	{
		const FPenetrationSceneCastWithExitHitsUsingSpeedResult& PenetrationSceneCastWithExitHitsUsingSpeedResult = PenetrationSceneCastWithExitHitsUsingSpeedResults[i];

		FVector RicochetTextOffsetDirection;

		// Collect locations and speeds
		TArray<TPair<FVector, float>> LocationsWithSpeeds;
		LocationsWithSpeeds.Emplace(PenetrationSceneCastWithExitHitsUsingSpeedResult.StartLocation, PenetrationSceneCastWithExitHitsUsingSpeedResult.StartSpeed);
		for (const FShooterHitResult& Hit : PenetrationSceneCastWithExitHitsUsingSpeedResult.HitResults)
		{
			LocationsWithSpeeds.Emplace(Hit.Location, Hit.Speed);

			if (Hit.bIsRicochet)
			{
				// This will be the axis that we shift our debug string locations for the pre-ricochet and post-ricochet speeds
				RicochetTextOffsetDirection = FVector::CrossProduct(PenetrationSceneCastWithExitHitsUsingSpeedResult.CastDirection, Hit.ImpactNormal);
			}
		}
		LocationsWithSpeeds.Emplace(PenetrationSceneCastWithExitHitsUsingSpeedResult.StopLocation, PenetrationSceneCastWithExitHitsUsingSpeedResult.StopSpeed);

		// Debug them
		const float RicochetOffsetAmount = 30.f;
		for (int j = 0; j < LocationsWithSpeeds.Num(); ++j)
		{
			if (LocationsWithSpeeds.IsValidIndex(j + 1) && LocationsWithSpeeds[j] == LocationsWithSpeeds[j + 1])
			{
				// Skip duplicates
				continue;
			}

			const FColor SpeedDebugColor = FPenetrationSceneCastWithExitHitsUsingSpeedResult::GetDebugColorForSpeed(LocationsWithSpeeds[j].Value, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);
			FVector StringLocation = LocationsWithSpeeds[j].Key;

			if (j == LocationsWithSpeeds.Num() - 1) // if we are the stop location
			{
				// If we are about to ricochet
				if (PenetrationSceneCastWithExitHitsUsingSpeedResult.HitResults.Num() > 0 && PenetrationSceneCastWithExitHitsUsingSpeedResult.HitResults.Last().bIsRicochet)
				{
					// Offset the pre-ricochet speed debug location upwards
					StringLocation += (-RicochetTextOffsetDirection * RicochetOffsetAmount);

					// If there is not a post-ricochet line after this
					if (i == PenetrationSceneCastWithExitHitsUsingSpeedResults.Num() - 1)
					{
						// Then just debug the post-ricochet speed here
						const FColor ExtraEndSpeedDebugColor = FPenetrationSceneCastWithExitHitsUsingSpeedResult::GetDebugColorForSpeed(EndSpeed, InInitialSpeed, FullSpeedColor, NoSpeedColor).ToFColor(true);
						const FVector ExtraEndStringLocation = LocationsWithSpeeds[j].Key + (RicochetTextOffsetDirection * RicochetOffsetAmount);
						const FString ExtraEndDebugString = FString::Printf(TEXT("%.2f"), EndSpeed);
						::DrawDebugString(InWorld, ExtraEndStringLocation, ExtraEndDebugString, nullptr, ExtraEndSpeedDebugColor, LifeTime);
					}
				}
			}
			else if (j == 0) // if we are the start location
			{
				// If we came from a ricochet
				if (PenetrationSceneCastWithExitHitsUsingSpeedResults.IsValidIndex(i - 1) && PenetrationSceneCastWithExitHitsUsingSpeedResults[i - 1].HitResults.Num() > 0 && PenetrationSceneCastWithExitHitsUsingSpeedResults[i - 1].HitResults.Last().bIsRicochet)
				{
					// Offset the post-ricochet speed debug location downwards
					StringLocation += (PreviousRicochetTextOffsetDirection * RicochetOffsetAmount);
				}
			}

			// Debug this location's speed
			const FString DebugString = FString::Printf(TEXT("%.2f"), LocationsWithSpeeds[j].Value);
			::DrawDebugString(InWorld, StringLocation, DebugString, nullptr, SpeedDebugColor, LifeTime);
		}

		PreviousRicochetTextOffsetDirection = RicochetTextOffsetDirection;
	}
}
