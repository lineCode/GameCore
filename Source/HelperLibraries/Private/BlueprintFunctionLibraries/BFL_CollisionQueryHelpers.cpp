// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"



const float UBFL_CollisionQueryHelpers::TraceStartWallAvoidancePadding = .01f; // good number for ensuring we don't start a trace on top of the object

bool UBFL_CollisionQueryHelpers::PenetrationLineTrace(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	// Ensure our collision query params do NOT ignore overlaps because we are tracing as an ECR_Overlap (otherwise we won't get any Hit Results)
	FCollisionQueryParams TraceCollisionQueryParams = InCollisionQueryParams;
	TraceCollisionQueryParams.bIgnoreTouches = false;

	// Perform the trace
	// Use ECR_Overlap to have this trace overlap through everything. Our CollisionResponseParams overrides all responses to the Trace Channel to overlap everything.
	// Also use their InTraceChannel to ensure that their ignored hits are ignored (because FCollisionResponseParams don't affect ECR_Ignore).
	InWorld->LineTraceMultiByChannel(OutHits, InTraceStart, InTraceEnd, InTraceChannel, TraceCollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));

	// Using ECollisionResponse::ECR_Overlap to trace was nice since we can get all hits (both overlap and blocking) in the segment without being stopped, but as a result, all of these hits have bBlockingHit as false.
	// So lets modify these hits to have the correct responses for the caller's trace channel.
	ChangeHitsResponseData(OutHits, InTraceChannel, InCollisionQueryParams);

	// Stop at any impenetrable hits
	if (IsHitImpenetrable != nullptr)
	{
		for (int32 i = 0; i < OutHits.Num(); ++i)
		{
			// Check the caller's stop condition for this hit
			if (IsHitImpenetrable(OutHits[i]))
			{
				// Remove the rest if there are any
				if (OutHits.IsValidIndex(i + 1))
				{
					OutHits.RemoveAt(i + 1, (OutHits.Num() - 1) - i);
				}

				return true;
			}
		}
	}

	// No impenetrable hits to stop us
	return false;
}

bool UBFL_CollisionQueryHelpers::PenetrationSweep(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	FCollisionQueryParams TraceCollisionQueryParams = InCollisionQueryParams;
	TraceCollisionQueryParams.bIgnoreTouches = false;

	InWorld->SweepMultiByChannel(OutHits, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, TraceCollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));

	ChangeHitsResponseData(OutHits, InTraceChannel, InCollisionQueryParams);

	if (IsHitImpenetrable != nullptr)
	{
		for (int32 i = 0; i < OutHits.Num(); ++i)
		{
			if (IsHitImpenetrable(OutHits[i]))
			{
				if (OutHits.IsValidIndex(i + 1))
				{
					OutHits.RemoveAt(i + 1, (OutHits.Num() - 1) - i);
				}

				return true;
			}
		}
	}

	return false;
}

bool UBFL_CollisionQueryHelpers::PenetrationLineTraceWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& ShouldNotPenetrate, const bool bUseBackwardsTraceOptimization)
{
	// FORWARDS TRACE to get our entrance hits
	TArray<FHitResult> EntranceHitResults;
	const bool bHitImpenetrableHit = PenetrationLineTrace(InWorld, EntranceHitResults, InTraceStart, InTraceEnd, InTraceChannel, InCollisionQueryParams, ShouldNotPenetrate);

	if (bUseBackwardsTraceOptimization && EntranceHitResults.Num() <= 0)
	{
		return bHitImpenetrableHit; // no entrance hits for our optimization to work with. Also this will always return false here
	}




	// BACKWARDS TRACE to get our exit hits
	const FVector BackwardsTraceStart = DetermineBackwardsTraceStart(EntranceHitResults, InTraceStart, InTraceEnd, bHitImpenetrableHit, bUseBackwardsTraceOptimization);
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	PenetrationLineTrace(InWorld, ExitHitResults, BackwardsTraceStart, InTraceStart, InTraceChannel, InCollisionQueryParams, ShouldNotPenetrate);



	// Make our exit hits relative to the forward trace
	MakeBackwardsHitsDataRelativeToForwadsTrace(ExitHitResults, InTraceStart, InTraceEnd, BackwardsTraceStart, bHitImpenetrableHit, bUseBackwardsTraceOptimization);

	// Lastly build our return value with the entrance and exit hits in order
	const FVector ForwardsTraceDir = (InTraceEnd - InTraceStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsTraceDir);

	return bHitImpenetrableHit;
}






// BEGIN private functions
void UBFL_CollisionQueryHelpers::ChangeHitsResponseData(TArray<FHitResult>& InOutHits, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams)
{
	for (int32 i = 0; i < InOutHits.Num(); ++i)
	{
		// Emulate the use of a trace channel by manually setting FHitResult::bBlockingHit and removing any hits that are ignored by the InTraceChannel
		if (const UPrimitiveComponent* PrimitiveComponent = InOutHits[i].Component.Get())
		{
			ECollisionChannel ComponentCollisionChannel;
			FCollisionResponseParams ComponentResponseParams;
			UCollisionProfile::GetChannelAndResponseParams(PrimitiveComponent->GetCollisionProfileName(), ComponentCollisionChannel, ComponentResponseParams);

			// The hit component's response to our InTraceChannel
			const ECollisionResponse CollisionResponse = ComponentResponseParams.CollisionResponse.GetResponse(InTraceChannel);

			if (CollisionResponse == ECollisionResponse::ECR_Block)
			{
				// This hit component blocks our InTraceChannel
				InOutHits[i].bBlockingHit = true;

				if (InCollisionQueryParams.bIgnoreBlocks)
				{
					// Ignore block
					InOutHits.RemoveAt(i);
					--i;
					continue;
				}
			}
			else if (CollisionResponse == ECollisionResponse::ECR_Overlap)
			{
				// This hit component overlaps our InTraceChannel
				InOutHits[i].bBlockingHit = false;

				if (InCollisionQueryParams.bIgnoreTouches)
				{
					// Ignore touch
					InOutHits.RemoveAt(i);
					--i;
					continue;
				}
			}
			else if (CollisionResponse == ECollisionResponse::ECR_Ignore)
			{
				// This hit component is ignored by our InTraceChannel
				UE_LOG(LogCollisionQueryHelpers, Warning, TEXT("%s() We got an ignored hit some how from our line trace. Removing it. This is weird because line traces should never return ignored hits as far as I know. This ocurred on Trace Channel: [%d]."), ANSI_TO_TCHAR(__FUNCTION__), static_cast<int32>(InTraceChannel));

				// Ignore this hit
				InOutHits.RemoveAt(i);
				--i;
				continue;
			}
		}
	}
}

FVector UBFL_CollisionQueryHelpers::DetermineBackwardsTraceStart(const TArray<FHitResult>& InForwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const bool bStoppedByHit, const bool bUseBackwardsTraceOptimization)
{
	const FVector ForwardDir = (InForwardsEnd - InForwardsStart).GetSafeNormal();
	
	if (bStoppedByHit)
	{
		// We hit an impenetrable hit, so we don't want to start the backwards trace past that hit's location
		FVector BackwardsTraceStart = InForwardsHitResults.Last().Location;

		// Bump us away from the hit location a little so that the backwards trace doesn't get stuck
		BackwardsTraceStart += ((ForwardDir * -1) * TraceStartWallAvoidancePadding);
		return BackwardsTraceStart;
	}


	if (!bUseBackwardsTraceOptimization)
	{
		// Start the backwards trace from the end of the forwards trace
		return InForwardsEnd + (ForwardDir * TraceStartWallAvoidancePadding);
	}


	// Instead of starting the backwards trace from the end of the forwards trace we can use an optimization to trim down the length. Ideally, we would start the backwards trace at
	// the last exit point but of course we don't have our exit points yet. But we CAN calculate the furthest possible exit point for each of our entrance points and choose the largest among them.

	// Find the furthest exit point that could possibly happen for each entrance hit and choose the largest among them
	FVector TheFurthestPossibleExitPoint = InForwardsStart;
	for (const FHitResult& HitResult : InForwardsHitResults)
	{
		if (const UPrimitiveComponent* HitComponent = HitResult.Component.Get())
		{
			const float MyBoundingDiameter = (HitComponent->Bounds.SphereRadius * 2);
			const FVector MyFurthestPossibleExitPoint = HitResult.Location + (ForwardDir * MyBoundingDiameter);

			// If my point is after the currently believed furthest point
			const bool bMyPointIsFurther = FVector::DotProduct(ForwardDir, (MyFurthestPossibleExitPoint - TheFurthestPossibleExitPoint)) > 0;
			if (bMyPointIsFurther)
			{
				TheFurthestPossibleExitPoint = MyFurthestPossibleExitPoint;
			}
		}
	}

	// Edge case: Our optimization turned out to make our backwards trace distance larger. Cap it.
	const bool bOptimizationWentPastTraceEnd = FVector::DotProduct(ForwardDir, (TheFurthestPossibleExitPoint - InForwardsEnd)) > 0;
	if (bOptimizationWentPastTraceEnd)
	{
		// Cap our backwards trace start
		TheFurthestPossibleExitPoint = InForwardsEnd;
	}

	// Furthest possible exit point of the penetration
	return TheFurthestPossibleExitPoint + (ForwardDir * TraceStartWallAvoidancePadding); // we use TraceStartWallAvoidancePadding here not to ensure that we don't hit the wall (b/c we do want the trace to hit it), but to just ensure we don't start inside it to remove possibility for unpredictable results. Very unlikely we hit a case where this padding is actually helpful here, but doing it to cover all cases.
}

void UBFL_CollisionQueryHelpers::MakeBackwardsHitsDataRelativeToForwadsTrace(TArray<FHitResult>& InOutBackwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const FVector& InBackwardsStart, const bool bStoppedByHit, const bool bUseBackwardsTraceOptimization)
{
	const float ForwardsTraceDistance = FVector::Distance(InForwardsStart, InForwardsEnd); // same as our pre-optimized backwards trace distance
	float BackwardsTraceDistance = ForwardsTraceDistance;
	if (bUseBackwardsTraceOptimization)
	{
		BackwardsTraceDistance = FVector::Distance(InBackwardsStart, InForwardsStart);
	}

	for (FHitResult& HitResult : InOutBackwardsHitResults)
	{
		// Assign expected values in our exit hit result
		HitResult.TraceStart = InForwardsStart;
		HitResult.TraceEnd = InForwardsEnd;

		// Remove/re-add our padding from this hit's distance
		HitResult.Distance += bStoppedByHit ? TraceStartWallAvoidancePadding : -TraceStartWallAvoidancePadding;

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

void UBFL_CollisionQueryHelpers::OrderHitResultsInForwardsDirection(TArray<FExitAwareHitResult>& OutOrderedHitResults, const TArray<FHitResult>& InEntranceHitResults, const TArray<FHitResult>& InExitHitResults, const FVector& InForwardsDirection)
{
	OutOrderedHitResults.Reserve(InEntranceHitResults.Num() + InExitHitResults.Num());

	int32 EntranceIndex = 0;
	int32 ExitIndex = InExitHitResults.Num() - 1;
	while (EntranceIndex < InEntranceHitResults.Num() || ExitIndex >= 0) // build our return value
	{
		bool bEntranceIsNext = true;
		if (EntranceIndex < InEntranceHitResults.Num() && ExitIndex >= 0) // if we have both entrance and exit hits to choose from
		{
			bEntranceIsNext = FVector::DotProduct(InForwardsDirection, (InExitHitResults[ExitIndex].Location - InEntranceHitResults[EntranceIndex].Location)) > 0; // figure out which hit to add to return value first
		}
		else if (EntranceIndex < InEntranceHitResults.Num())	// if we only have entrances left
		{
			bEntranceIsNext = true;
		}
		else if (ExitIndex >= 0)								// if we only have exits left
		{
			bEntranceIsNext = false;
		}

		if (bEntranceIsNext)
		{
			// Add this entrance hit
			FExitAwareHitResult HitResult = InEntranceHitResults[EntranceIndex];
			HitResult.bIsExitHit = false;
			OutOrderedHitResults.Add(HitResult);
			++EntranceIndex; // don't consider this entrance anymore because we added it to return value
			continue;
		}
		else
		{
			// Add this exit hit
			FExitAwareHitResult HitResult = InExitHitResults[ExitIndex];
			HitResult.bIsExitHit = true;
			OutOrderedHitResults.Add(HitResult);
			--ExitIndex; // don't consider this exit anymore because we added it to return value
			continue;
		}
	}
}
// END private functions
