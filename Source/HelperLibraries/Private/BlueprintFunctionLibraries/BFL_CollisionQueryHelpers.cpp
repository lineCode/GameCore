// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "Utilities/HLLogCategories.h"



bool UBFL_CollisionQueryHelpers::LineTraceMultiByChannelWithPenetrations(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& ShouldStopAtHit)
{
	// Ensure our collision query params do NOT ignore overlaps because we are tracing as an ECR_Overlap (otherwise we won't get any Hit Results)
	FCollisionQueryParams TraceCollisionQueryParams = InCollisionQueryParams;
	TraceCollisionQueryParams.bIgnoreTouches = false;

	// Perform the trace
	// Use ECR_Overlap to have this trace overlap through everything. Our CollisionResponseParams overrides all responses to the Trace Channel to overlap everything.
	// Also use their InTraceChannel to ensure that their ignored hits are ignored (because FCollisionResponseParams don't affect ECR_Ignore).
	InWorld->LineTraceMultiByChannel(OutHits, InTraceStart, InTraceEnd, InTraceChannel, TraceCollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));

	for (int32 i = 0; i < OutHits.Num(); ++i)
	{
		// Emulate the use of a trace channel by manually setting FHitResult::bBlockingHit and removing any hits that are ignored by the InTraceChannel
		if (const UPrimitiveComponent* PrimitiveComponent = OutHits[i].Component.Get())
		{
			ECollisionChannel ComponentCollisionChannel;
			FCollisionResponseParams ComponentResponseParams;
			UCollisionProfile::GetChannelAndResponseParams(PrimitiveComponent->GetCollisionProfileName(), ComponentCollisionChannel, ComponentResponseParams);

			// The hit component's response to our InTraceChannel
			const ECollisionResponse CollisionResponse = ComponentResponseParams.CollisionResponse.GetResponse(InTraceChannel);

			if (CollisionResponse == ECollisionResponse::ECR_Block)
			{
				// This hit component blocks our InTraceChannel
				OutHits[i].bBlockingHit = true;

				if (InCollisionQueryParams.bIgnoreBlocks)
				{
					// Ignore block
					OutHits.RemoveAt(i);
					--i;
					continue;
				}
			}
			else if (CollisionResponse == ECollisionResponse::ECR_Overlap)
			{
				// This hit component overlaps our InTraceChannel
				OutHits[i].bBlockingHit = false;

				if (InCollisionQueryParams.bIgnoreTouches)
				{
					// Ignore touch
					OutHits.RemoveAt(i);
					--i;
					continue;
				}
			}
			else if (CollisionResponse == ECollisionResponse::ECR_Ignore)
			{
				// This hit component is ignored by our InTraceChannel
				UE_LOG(LogCollisionQueryHelpers, Warning, TEXT("%s() We got an ignored hit some how from our line trace. Removing it. This is weird because line traces should never return ignored hits as far as I know. This ocurred on Trace Channel: [%d]."), ANSI_TO_TCHAR(__FUNCTION__), static_cast<int32>(InTraceChannel));

				// Ignore this hit
				OutHits.RemoveAt(i);
				--i;
				continue;
			}
		}

		if (ShouldStopAtHit(OutHits[i])) // run caller's function to see if they want this hit result to be the last one
		{
			// Remove the rest if there are any
			if (OutHits.IsValidIndex(i + 1))
			{
				OutHits.RemoveAt(i + 1, (OutHits.Num() - 1) - i);
			}

			return true;
		}
	}

	// No impenetrable hits to stop us
	return false;
}

bool UBFL_CollisionQueryHelpers::ExitAwareLineTraceMultiByChannelWithPenetrations(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& ShouldNotPenetrate, const bool bUseBackwardsTraceOptimization)
{
	// Forwards trace to get our entrance hits
	TArray<FHitResult> EntranceHitResults;
	const bool bHitImpenetrableHit = LineTraceMultiByChannelWithPenetrations(InWorld, EntranceHitResults, InTraceStart, InTraceEnd, InTraceChannel, InCollisionQueryParams, ShouldNotPenetrate);

	if (bUseBackwardsTraceOptimization && EntranceHitResults.Num() <= 0)
	{
		return bHitImpenetrableHit; // no entrance hits for our optimization to work with. Also this will always return false here
	}


	// Calculate BackwardsTraceStart
	const FVector ForwardsTraceDir = (InTraceEnd - InTraceStart).GetSafeNormal();
	FVector BackwardsTraceStart;
	const float TraceStartWallAvoidancePadding = .01f; // good number for ensuring we don't start a trace on top of the object
	if (bHitImpenetrableHit)
	{
		// We hit an impenetrable hit, so we don't want to start the backwards trace past that hit's location
		BackwardsTraceStart = EntranceHitResults.Last().Location;

		// Bump us away from the hit location a little so that the backwards trace doesn't get stuck
		BackwardsTraceStart += ((ForwardsTraceDir * -1) * TraceStartWallAvoidancePadding);
	}
	else
	{
		if (!bUseBackwardsTraceOptimization)
		{
			// Start the backwards trace from the end of the forwards trace
			BackwardsTraceStart = InTraceEnd + (ForwardsTraceDir * TraceStartWallAvoidancePadding);
		}
		else
		{
			// Instead of starting the backwards trace from the end of the forwards trace we can use an optimization to trim down the length. Ideally, we would start the backwards trace at
			// the last exit point but of course we don't have our exit points yet. But we CAN calculate the furthest possible exit point for each of our entrance points and choose the largest among them.

			// Find the furthest exit point that could possibly happen for each entrance hit and choose the largest among them
			FVector TheFurthestPossibleExitPoint = InTraceStart;
			for (const FHitResult& HitResult : EntranceHitResults)
			{
				if (const UPrimitiveComponent* HitComponent = HitResult.Component.Get())
				{
					const float MyBoundingDiameter = (HitComponent->Bounds.SphereRadius * 2);
					const FVector MyFurthestPossibleExitPoint = HitResult.Location + (ForwardsTraceDir * MyBoundingDiameter);

					// If my point is after the currently believed furthest point
					const bool bMyPointIsFurther = FVector::DotProduct(ForwardsTraceDir, (MyFurthestPossibleExitPoint - TheFurthestPossibleExitPoint)) > 0;
					if (bMyPointIsFurther)
					{
						TheFurthestPossibleExitPoint = MyFurthestPossibleExitPoint;
					}
				}
			}

			// Edge case: Our optimization turned out to make our backwards trace distance larger. Cap it.
			const bool bOptimizationWentPastTraceEnd = FVector::DotProduct(ForwardsTraceDir, (TheFurthestPossibleExitPoint - InTraceEnd)) > 0;
			if (bOptimizationWentPastTraceEnd)
			{
				// Cap our backwards trace start
				TheFurthestPossibleExitPoint = InTraceEnd;
			}

			// Furthest possible exit point of the penetration
			BackwardsTraceStart = TheFurthestPossibleExitPoint + (ForwardsTraceDir * TraceStartWallAvoidancePadding); // we use TraceStartWallAvoidancePadding here not to ensure that we don't hit the wall (b/c we do want the trace to hit it), but to just ensure we don't start inside it to remove possibility for unpredictable results. Very unlikely we hit a case where this padding is actually helpful here, but doing it to cover all cases.
		}
	}

	// Backwards trace to get our exit hits
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	LineTraceMultiByChannelWithPenetrations(InWorld, ExitHitResults, BackwardsTraceStart, InTraceStart, InTraceChannel, InCollisionQueryParams, ShouldNotPenetrate);

	// Make our exit hits relative to the forward trace
	{
		const float ForwardsTraceDistance = FVector::Distance(InTraceStart, InTraceEnd); // same as our pre-optimized backwards trace distance
		float BackwardsTraceDistance = ForwardsTraceDistance;
		if (bUseBackwardsTraceOptimization)
		{
			BackwardsTraceDistance = FVector::Distance(BackwardsTraceStart, InTraceStart);
		}

		for (FHitResult& HitResult : ExitHitResults)
		{
			// Assign expected values in our exit hit result
			HitResult.TraceStart = InTraceStart;
			HitResult.TraceEnd = InTraceEnd;

			// Remove/re-add our padding from this hit's distance
			HitResult.Distance += bHitImpenetrableHit ? TraceStartWallAvoidancePadding : -TraceStartWallAvoidancePadding;

			if (bUseBackwardsTraceOptimization)
			{
				// Account for the distance that was optimized out in the backwards trace
				HitResult.Distance += (ForwardsTraceDistance - BackwardsTraceDistance);
			}

			// Make the distance relative to the forwards direction
			HitResult.Distance = (ForwardsTraceDistance - HitResult.Distance); // a oneminus-like operation

			// Now that we have corrected Distance, recalculate Time to be correct
			HitResult.Time = FMath::GetRangePct(FVector2D(0.f, ForwardsTraceDistance), HitResult.Distance);
		}
	}


	// Lastly build our return value with the entrance and exit hits in order
	OutHits.Reserve(EntranceHitResults.Num() + ExitHitResults.Num());
	int32 EntranceIndex = 0;
	while (EntranceIndex < EntranceHitResults.Num() || ExitHitResults.Num() > 0) // build our return value
	{
		bool bEntranceIsNext = true;
		if (EntranceIndex < EntranceHitResults.Num() && ExitHitResults.Num() > 0) // if we have both entrance and exit hits to choose from
		{
			bEntranceIsNext = FVector::DotProduct(ForwardsTraceDir, (ExitHitResults.Last().Location - EntranceHitResults[EntranceIndex].Location)) > 0; // figure out which hit to add to return value first
		}
		else if (EntranceIndex < EntranceHitResults.Num())	// if we only have entrances left
		{
			bEntranceIsNext = true;
		}
		else if (ExitHitResults.Num() > 0)					// if we only have exits left
		{
			bEntranceIsNext = false;
		}

		if (bEntranceIsNext)
		{
			// Add this entrance hit
			FExitAwareHitResult HitResult = EntranceHitResults[EntranceIndex];
			HitResult.bIsExitHit = false;
			OutHits.Add(HitResult);
			++EntranceIndex; // don't consider this entrance anymore because we added it to return value
			continue;
		}
		else
		{
			// Add this exit hit
			FExitAwareHitResult HitResult = ExitHitResults.Last();
			HitResult.bIsExitHit = true;
			OutHits.Add(HitResult);
			ExitHitResults.RemoveAt(ExitHitResults.Num() - 1); // don't consider this exit anymore because we added it to return value
			continue;
		}
	}

	return bHitImpenetrableHit;
}


FVector UBFL_CollisionQueryHelpers::GetLocationAimDirection(const UWorld* World, const FCollisionQueryParams& QueryParams, const FVector& AimPoint, const FVector& AimDir, const float& MaxRange, const FVector& Location)
{
	if (Location.Equals(AimPoint))
	{
		// The Location is the same as the AimPoint so we can skip the camera trace and just return the AimDir as the muzzle's direction
		return AimDir;
	}

	// Line trace from the Location to the point that AimDir is looking at
	FCollisionQueryParams CollisionQueryParams = QueryParams;
	CollisionQueryParams.bIgnoreTouches = true;

	FVector TraceEnd = AimPoint + (AimDir * MaxRange);

	FHitResult HitResult;
	const bool bSuccess = World->LineTraceSingleByChannel(HitResult, AimPoint, TraceEnd, ECollisionChannel::ECC_Visibility, CollisionQueryParams);

	if (!bSuccess)
	{
		// AimDir is not looking at anything for our Location to point to
		return (TraceEnd - Location).GetSafeNormal();
	}

	// Return the direction from the Location to the point that the Player is looking at
	return (HitResult.Location - Location).GetSafeNormal();
}
