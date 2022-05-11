// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "BlueprintFunctionLibraries/BFL_MathHelpers.h"



const float UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding = .01f; // good number for bumping a scene cast start location away from the surface of geometry


// BEGIN penetration queries
bool UBFL_CollisionQueryHelpers::PenetrationSceneCast(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	// Ensure our collision query params do NOT ignore overlaps because we are tracing as an ECR_Overlap (otherwise we won't get any Hit Results)
	FCollisionQueryParams CollisionQueryParams = InCollisionQueryParams;
	CollisionQueryParams.bIgnoreTouches = false;

	// Perform the trace/sweep
	// Use ECR_Overlap to have this scene cast overlap through everything. Our CollisionResponseParams overrides all responses to the Trace Channel to overlap everything.
	// Also use their InTraceChannel to ensure that their ignored hits are ignored (because FCollisionResponseParams don't affect ECR_Ignore).
	SceneCastMultiByChannel(InWorld, OutHits, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, CollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));

	// Using ECollisionResponse::ECR_Overlap to scene cast was nice since we can get all hits (both overlap and blocking) in the segment without being stopped, but as a result, all of these hits have bBlockingHit as false.
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
bool UBFL_CollisionQueryHelpers::PenetrationLineTrace(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable)
{
	FCollisionShape LineShape = FCollisionShape(); // default constructor makes a line shape for us. I would want to use their FCollisionShape::LineShape but the engine doesn't seem to expose it for modules
	return PenetrationSceneCast(InWorld, OutHits, InTraceStart, InTraceEnd, FQuat::Identity, InTraceChannel, LineShape, InCollisionQueryParams, IsHitImpenetrable);
}
// END penetration queries

// BEGIN queries with exit hits
bool UBFL_CollisionQueryHelpers::SceneCastWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const bool bOptimizeBackwardsSceneCastLength)
{
	// FORWARDS SCENE CAST to get our entrance hits
	TArray<FHitResult> EntranceHitResults;
	const bool bHitBlockingHit = SceneCastMultiByChannel(InWorld, EntranceHitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitBlockingHit; // no entrance hits for our optimization to work with. Also this will always return false here
	}


	// BACKWARDS SCENE CAST to get our exit hits
	const FVector BackwardsStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InStart, InEnd, bHitBlockingHit, bOptimizeBackwardsSceneCastLength, UBFL_MathHelpers::GetCollisionShapeBoundingSphereRadius(InCollisionShape));
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	SceneCastMultiByChannel(InWorld, ExitHitResults, BackwardsStart, InStart, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams);


	// Make our exit hits relative to the forwards cast
	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, InStart, InEnd, BackwardsStart, bHitBlockingHit, bOptimizeBackwardsSceneCastLength);

	// Lastly combine these hits together into our output value with the entrance and exit hits in order
	const FVector ForwardsDir = (InEnd - InStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsDir);

	return bHitBlockingHit;
}
bool UBFL_CollisionQueryHelpers::LineTraceMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const bool bOptimizeBackwardsSceneCastLength)
{
	FCollisionShape LineShape = FCollisionShape();
	return SceneCastWithExitHits(InWorld, OutHits, InTraceStart, InTraceEnd, FQuat::Identity, InTraceChannel, LineShape, InCollisionQueryParams, bOptimizeBackwardsSceneCastLength);
}

bool UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength)
{
	TArray<FHitResult> EntranceHitResults;
	const bool bHitImpenetrableHit = PenetrationSceneCast(InWorld, EntranceHitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitImpenetrableHit;
	}


	const FVector BackwardsStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InStart, InEnd, bHitImpenetrableHit, bOptimizeBackwardsSceneCastLength, UBFL_MathHelpers::GetCollisionShapeBoundingSphereRadius(InCollisionShape));
	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	PenetrationSceneCast(InWorld, ExitHitResults, BackwardsStart, InStart, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable);


	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, InStart, InEnd, BackwardsStart, bHitImpenetrableHit, bOptimizeBackwardsSceneCastLength);

	const FVector ForwardsDir = (InEnd - InStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsDir);

	return bHitImpenetrableHit;
}
bool UBFL_CollisionQueryHelpers::PenetrationLineTraceWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength)
{
	FCollisionShape LineShape = FCollisionShape();
	return PenetrationSceneCastWithExitHits(InWorld, OutHits, InTraceStart, InTraceEnd, FQuat::Identity, InTraceChannel, LineShape, InCollisionQueryParams, IsHitImpenetrable, bOptimizeBackwardsSceneCastLength);
}
// END queries with exit hits


// BEGIN private functions
bool UBFL_CollisionQueryHelpers::SceneCastMultiByChannel(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams)
{
	// UWorld has SweepMultiByChannel() which already checks for zero extent shapes, but it doesn't explicitly check for ECollisionChannel::LineShape and its name can lead you to think that it doesn't support line traces
	if (InCollisionShape.ShapeType == ECollisionShape::Line)
	{
		return InWorld->LineTraceMultiByChannel(OutHits, InStart, InEnd, InTraceChannel, InCollisionQueryParams, InCollisionResponseParams);
	}
	else
	{
		return InWorld->SweepMultiByChannel(OutHits, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams);
	}
}

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

FVector UBFL_CollisionQueryHelpers::DetermineBackwardsSceneCastStart(const TArray<FHitResult>& InForwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const bool bStoppedByHit, const bool bOptimizeBackwardsSceneCastLength, const float SweepShapeBoundingSphereRadius)
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
		return InForwardsEnd;
	}


	// Instead of starting the backwards scene cast from the end of the forwards scene cast we can use an optimization to trim down the length. Ideally, we would start the backwards scene cast at
	// the last exit location but of course we don't have our exit locations yet. But we CAN calculate the furthest possible exit location for each of our entrance points and choose the largest among them.

	// Find the furthest exit location that could possibly happen for each entrance hit and choose the furthest among them
	FVector TheFurthestPossibleExitLocation = InForwardsStart;
	for (const FHitResult& HitResult : InForwardsHitResults)
	{
		if (const UPrimitiveComponent* HitComponent = HitResult.Component.Get())
		{
			const float MyBoundingDiameter = (HitComponent->Bounds.SphereRadius * 2);
			const FVector MyFurthestPossibleExitLocation = HitResult.Location + (ForwardDir * MyBoundingDiameter);

			// If my point is after the currently believed furthest point
			const bool bMyPointIsFurther = FVector::DotProduct(ForwardDir, (MyFurthestPossibleExitLocation - TheFurthestPossibleExitLocation)) > 0;
			if (bMyPointIsFurther)
			{
				TheFurthestPossibleExitLocation = MyFurthestPossibleExitLocation;
			}
		}
	}

	// The optimal backwards start gives a minimal scene cast distance that can still cover the furthest possible exit location
	FVector OptimizedBackwardsSceneCastStart = TheFurthestPossibleExitLocation;
	// Bump us forwards to ensure a potential exit at the furthest possible exit location can get hit properly
	OptimizedBackwardsSceneCastStart += (ForwardDir * SweepShapeBoundingSphereRadius); // bump us forwards by any sweep shapes' bounding sphere radius so that the sweep geometry starts past TheFurthestPossibleExitLocation
	OptimizedBackwardsSceneCastStart += (ForwardDir * SceneCastStartWallAvoidancePadding); // bump us forwards by the SceneCastStartWallAvoidancePadding so that backwards scene cast does not start on top of TheFurthestPossibleExitLocation

	// Edge case: Our optimization turned out to make our backwards scene cast distance larger. Cap it.
	const bool bOptimizationWentPastForwardEnd = FVector::DotProduct(ForwardDir, (OptimizedBackwardsSceneCastStart - InForwardsEnd)) > 0;
	if (bOptimizationWentPastForwardEnd)
	{
		// Cap our backwards scene cast start
		return InForwardsEnd;
	}

	return OptimizedBackwardsSceneCastStart;
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
