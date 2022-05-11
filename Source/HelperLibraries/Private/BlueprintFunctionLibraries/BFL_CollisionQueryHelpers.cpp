// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"



const float UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding = .01f; // good number for ensuring we don't start a scene cast on top of the object


// BEGIN penetration queries
bool UBFL_CollisionQueryHelpers::PenetrationLineTrace(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	// Ensure our collision query params do NOT ignore overlaps because we are tracing as an ECR_Overlap (otherwise we won't get any Hit Results)
	FCollisionQueryParams CollisionQueryParams = InCollisionQueryParams;
	CollisionQueryParams.bIgnoreTouches = false;

	// Perform the trace
	// Use ECR_Overlap to have this trace overlap through everything. Our CollisionResponseParams overrides all responses to the Trace Channel to overlap everything.
	// Also use their InTraceChannel to ensure that their ignored hits are ignored (because FCollisionResponseParams don't affect ECR_Ignore).
	InWorld->LineTraceMultiByChannel(OutHits, InTraceStart, InTraceEnd, InTraceChannel, CollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));

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
	FCollisionQueryParams CollisionQueryParams = InCollisionQueryParams;
	CollisionQueryParams.bIgnoreTouches = false;

	InWorld->SweepMultiByChannel(OutHits, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, CollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));

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
// END penetration queries

// BEGIN queries with exit hits
bool UBFL_CollisionQueryHelpers::PenetrationLineTraceWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength)
{
	// FORWARDS TRACE to get our entrance hits
	TArray<FHitResult> EntranceHitResults;
	const bool bHitImpenetrableHit = PenetrationLineTrace(InWorld, EntranceHitResults, InTraceStart, InTraceEnd, InTraceChannel, InCollisionQueryParams, IsHitImpenetrable);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitImpenetrableHit; // no entrance hits for our optimization to work with. Also this will always return false here
	}


	// BACKWARDS TRACE to get our exit hits
	const FVector BackwardsTraceStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InTraceStart, InTraceEnd, bHitImpenetrableHit, bOptimizeBackwardsSceneCastLength);
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	PenetrationLineTrace(InWorld, ExitHitResults, BackwardsTraceStart, InTraceStart, InTraceChannel, InCollisionQueryParams, IsHitImpenetrable);


	// Make our exit hits relative to the forwards trace
	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, InTraceStart, InTraceEnd, BackwardsTraceStart, bHitImpenetrableHit, bOptimizeBackwardsSceneCastLength);

	// Lastly combine these hits together into our output value with the entrance and exit hits in order
	const FVector ForwardsTraceDir = (InTraceEnd - InTraceStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsTraceDir);

	return bHitImpenetrableHit;
}

bool UBFL_CollisionQueryHelpers::PenetrationSweepWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength)
{
	TArray<FHitResult> EntranceHitResults;
	const bool bHitImpenetrableHit = PenetrationSweep(InWorld, EntranceHitResults, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitImpenetrableHit;
	}


	const FVector BackwardsSweepStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InSweepStart, InSweepEnd, bHitImpenetrableHit, bOptimizeBackwardsSceneCastLength);
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	PenetrationSweep(InWorld, ExitHitResults, BackwardsSweepStart, InSweepStart, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable);


	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, InSweepStart, InSweepEnd, BackwardsSweepStart, bHitImpenetrableHit, bOptimizeBackwardsSceneCastLength);

	const FVector ForwardsSweepDir = (InSweepEnd - InSweepStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsSweepDir);

	return bHitImpenetrableHit;
}

bool UBFL_CollisionQueryHelpers::LineTraceMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const bool bOptimizeBackwardsSceneCastLength)
{
	TArray<FHitResult> EntranceHitResults;
	const bool bHitBlockingHit = InWorld->LineTraceMultiByChannel(EntranceHitResults, InTraceStart, InTraceEnd, InTraceChannel, InCollisionQueryParams);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitBlockingHit;
	}


	const FVector BackwardsTraceStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InTraceStart, InTraceEnd, bHitBlockingHit, bOptimizeBackwardsSceneCastLength);
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	InWorld->LineTraceMultiByChannel(ExitHitResults, BackwardsTraceStart, InTraceStart, InTraceChannel, InCollisionQueryParams);


	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, InTraceStart, InTraceEnd, BackwardsTraceStart, bHitBlockingHit, bOptimizeBackwardsSceneCastLength);

	const FVector ForwardsTraceDir = (InTraceEnd - InTraceStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsTraceDir);

	return bHitBlockingHit;
}

bool UBFL_CollisionQueryHelpers::SweepMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const bool bOptimizeBackwardsSceneCastLength)
{
	TArray<FHitResult> EntranceHitResults;
	const bool bHitBlockingHit = InWorld->SweepMultiByChannel(EntranceHitResults, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitBlockingHit;
	}


	const FVector BackwardsSweepStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InSweepStart, InSweepEnd, bHitBlockingHit, bOptimizeBackwardsSceneCastLength);
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	InWorld->SweepMultiByChannel(ExitHitResults, BackwardsSweepStart, InSweepStart, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams);


	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, InSweepStart, InSweepEnd, BackwardsSweepStart, bHitBlockingHit, bOptimizeBackwardsSceneCastLength);

	const FVector ForwardsSweepDir = (InSweepEnd - InSweepStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsSweepDir);

	return bHitBlockingHit;
}
// END queries with exit hits


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
				UE_LOG(LogCollisionQueryHelpers, Warning, TEXT("%s() We got an ignored hit some how from our scene cast. Removing it. This is weird because scene casts should never return ignored hits as far as I know. This ocurred on Trace Channel: [%d]."), ANSI_TO_TCHAR(__FUNCTION__), static_cast<int32>(InTraceChannel));

				// Ignore this hit
				InOutHits.RemoveAt(i);
				--i;
				continue;
			}
		}
	}
}

FVector UBFL_CollisionQueryHelpers::DetermineBackwardsSceneCastStart(const TArray<FHitResult>& InForwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const bool bStoppedByHit, const bool bOptimizeBackwardsSceneCastLength)
{
	const FVector ForwardDir = (InForwardsEnd - InForwardsStart).GetSafeNormal();
	
	if (bStoppedByHit)
	{
		// We hit an impenetrable hit, so we don't want to start the backwards scene cast past that hit's location
		FVector BackwardsSceneCastStart = InForwardsHitResults.Last().Location;

		// Bump us away from the hit location a little so that the backwards scene cast doesn't get stuck
		BackwardsSceneCastStart += ((ForwardDir * -1) * SceneCastStartWallAvoidancePadding);
		return BackwardsSceneCastStart;
	}


	if (!bOptimizeBackwardsSceneCastLength)
	{
		// Start the backwards scene cast from the end of the forwards scene cast
		return InForwardsEnd + (ForwardDir * SceneCastStartWallAvoidancePadding);
	}


	// Instead of starting the backwards scene cast from the end of the forwards scene cast we can use an optimization to trim down the length. Ideally, we would start the backwards scene cast at
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

	// Edge case: Our optimization turned out to make our backwards scene cast distance larger. Cap it.
	const bool bOptimizationWentPastForwardEnd = FVector::DotProduct(ForwardDir, (TheFurthestPossibleExitPoint - InForwardsEnd)) > 0;
	if (bOptimizationWentPastForwardEnd)
	{
		// Cap our backwards scene cast start
		TheFurthestPossibleExitPoint = InForwardsEnd;
	}

	// Furthest possible exit point of the penetration
	return TheFurthestPossibleExitPoint + (ForwardDir * SceneCastStartWallAvoidancePadding); // we use SceneCastStartWallAvoidancePadding here not to ensure that we don't hit the wall (b/c we do want the scene cast to hit it), but to just ensure we don't start inside it to remove possibility for unpredictable results. Very unlikely we hit a case where this padding is actually helpful here, but doing it to cover all cases.
}

void UBFL_CollisionQueryHelpers::MakeBackwardsHitsDataRelativeToForwadsSceneCast(TArray<FHitResult>& InOutBackwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const FVector& InBackwardsStart, const bool bStoppedByHit, const bool bOptimizeBackwardsSceneCastLength)
{
	const float ForwardsSceneCastDistance = FVector::Distance(InForwardsStart, InForwardsEnd); // same as our pre-optimized backwards scene cast distance
	float BackwardsSceneCastDistance = ForwardsSceneCastDistance;
	if (bOptimizeBackwardsSceneCastLength)
	{
		BackwardsSceneCastDistance = FVector::Distance(InBackwardsStart, InForwardsStart);
	}

	for (FHitResult& HitResult : InOutBackwardsHitResults)
	{
		// Assign expected values in our exit hit result
		HitResult.TraceStart = InForwardsStart;
		HitResult.TraceEnd = InForwardsEnd;

		// Remove/re-add our padding from this hit's distance
		HitResult.Distance += bStoppedByHit ? SceneCastStartWallAvoidancePadding : -SceneCastStartWallAvoidancePadding;

		if (bOptimizeBackwardsSceneCastLength)
		{
			// Account for the distance that was optimized out in the backwards scene cast
			HitResult.Distance += (ForwardsSceneCastDistance - BackwardsSceneCastDistance);
		}

		// Make the distance relative to the forwards direction
		HitResult.Distance = (ForwardsSceneCastDistance - HitResult.Distance); // a oneminus-like operation

		// Now that we have corrected Distance, recalculate Time to be correct
		HitResult.Time = FMath::GetRangePct(FVector2D(0.f, ForwardsSceneCastDistance), HitResult.Distance);
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
