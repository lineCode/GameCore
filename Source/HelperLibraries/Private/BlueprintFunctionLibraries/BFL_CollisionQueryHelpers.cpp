// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "BlueprintFunctionLibraries/BFL_MathHelpers.h"
#include "BlueprintFunctionLibraries/BFL_ArrayHelpers.h"
#include "BlueprintFunctionLibraries/BFL_DrawDebugHelpers.h"
#include "DrawDebugHelpers.h"
#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"



const float UBFL_CollisionQueryHelpers::SceneCastStartWallAvoidancePadding = .01f; // good number for bumping a scene cast start location away from the surface of geometry
const TFunctionRef<bool(const FHitResult&)>& UBFL_CollisionQueryHelpers::DefaultIsHitImpenetrable = [](const FHitResult&) { return false; };


//  BEGIN Custom query
bool UBFL_CollisionQueryHelpers::SceneCastMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const bool bOptimizeBackwardsSceneCastLength, const bool bDrawDebugForBackwardsStart)
{
	// FORWARDS SCENE CAST to get our entrance hits
	TArray<FHitResult> EntranceHitResults;
	const bool bHitBlockingHit = SceneCastMultiByChannel(InWorld, EntranceHitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return bHitBlockingHit; // no entrance hits for our optimization to work with. Also this will always return false here
	}


	// BACKWARDS SCENE CAST to get our exit hits
	const FVector BackwardsStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InStart, InEnd, (bHitBlockingHit ? &EntranceHitResults.Last() : nullptr), bOptimizeBackwardsSceneCastLength, UBFL_MathHelpers::GetCollisionShapeBoundingSphereRadius(InCollisionShape));
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugForBackwardsStart)
	{
		const FVector BackwardsDir = (InStart - BackwardsStart).GetSafeNormal();
		DrawDebugForBackwardsStart(InWorld, InCollisionShape, InRotation, BackwardsStart, BackwardsDir);
	}
#endif // ENABLE_DRAW_DEBUG
	FCollisionQueryParams BackwardsCollisionQueryParams = InCollisionQueryParams;
	BackwardsCollisionQueryParams.bFindInitialOverlaps = false;

	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	SceneCastMultiByChannel(InWorld, ExitHitResults, BackwardsStart, InStart, InRotation, InTraceChannel, InCollisionShape, BackwardsCollisionQueryParams, InCollisionResponseParams);

	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, EntranceHitResults);


	// Lastly combine these hits together into our output value with the entrance and exit hits in order
	const FVector ForwardsDir = (InEnd - InStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsDir);

	return bHitBlockingHit;
}
bool UBFL_CollisionQueryHelpers::LineTraceMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const bool bOptimizeBackwardsSceneCastLength, const bool bDrawDebugForBackwardsStart)
{
	FCollisionShape LineShape = FCollisionShape();
	return SceneCastMultiWithExitHits(InWorld, OutHits, InTraceStart, InTraceEnd, FQuat::Identity, InTraceChannel, LineShape, InCollisionQueryParams, InCollisionResponseParams, bOptimizeBackwardsSceneCastLength, bDrawDebugForBackwardsStart);
}
bool UBFL_CollisionQueryHelpers::SweepMultiWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams, const bool bOptimizeBackwardsSceneCastLength, const bool bDrawDebugForBackwardsStart)
{
	UE_CLOG(InCollisionShape.IsLine(), LogCollisionQueryHelpers, Warning, TEXT("%s() was used with a FCollisionShape::LineShape. Use the linetrace version if you want a line traces."), ANSI_TO_TCHAR(__FUNCTION__));
	return SceneCastMultiWithExitHits(InWorld, OutHits, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, InCollisionResponseParams, bOptimizeBackwardsSceneCastLength, bDrawDebugForBackwardsStart);
}
//  END Custom query


//  BEGIN Custom query
FHitResult* UBFL_CollisionQueryHelpers::PenetrationSceneCast(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	// Ensure our collision query params do NOT ignore overlaps because we are scene casting as an ECR_Overlap (otherwise we won't get any Hit Results)
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
	for (int32 i = 0; i < OutHits.Num(); ++i)
	{
		if (IsHitImpenetrable(OutHits[i]))
		{
			// Remove the rest if there are any
			if (OutHits.IsValidIndex(i + 1))
			{
				UBFL_ArrayHelpers::RemoveTheRestAt(OutHits, i + 1);
			}

			return &OutHits[i];
		}
	}

	// No impenetrable hits stopped us
	return nullptr;
}
FHitResult* UBFL_CollisionQueryHelpers::PenetrationLineTrace(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	FCollisionShape LineShape = FCollisionShape(); // default constructor makes a line shape for us. I would want to use their FCollisionShape::LineShape but the engine doesn't seem to expose it for modules
	return PenetrationSceneCast(InWorld, OutHits, InTraceStart, InTraceEnd, FQuat::Identity, InTraceChannel, LineShape, InCollisionQueryParams, IsHitImpenetrable);
}
FHitResult* UBFL_CollisionQueryHelpers::PenetrationSweep(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable)
{
	UE_CLOG(InCollisionShape.IsLine(), LogCollisionQueryHelpers, Warning, TEXT("%s() was used with a FCollisionShape::LineShape. Use the linetrace version if you want a line traces."), ANSI_TO_TCHAR(__FUNCTION__));
	return PenetrationSceneCast(InWorld, OutHits, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable);
}
//  END Custom query


//  BEGIN Custom query
FExitAwareHitResult* UBFL_CollisionQueryHelpers::PenetrationSceneCastWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength, const bool bDrawDebugForBackwardsStart)
{
	TArray<FHitResult> EntranceHitResults;
	FHitResult* ImpenetrableHit = PenetrationSceneCast(InWorld, EntranceHitResults, InStart, InEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable);
	if (bOptimizeBackwardsSceneCastLength && EntranceHitResults.Num() <= 0)
	{
		return nullptr;
	}


	const FVector BackwardsStart = DetermineBackwardsSceneCastStart(EntranceHitResults, InStart, InEnd, ImpenetrableHit, bOptimizeBackwardsSceneCastLength, UBFL_MathHelpers::GetCollisionShapeBoundingSphereRadius(InCollisionShape));
#if ENABLE_DRAW_DEBUG
	if (bDrawDebugForBackwardsStart)
	{
		const FVector BackwardsDir = (InStart - BackwardsStart).GetSafeNormal();
		DrawDebugForBackwardsStart(InWorld, InCollisionShape, InRotation, BackwardsStart, BackwardsDir);
	}
#endif // ENABLE_DRAW_DEBUG
	FCollisionQueryParams BackwardsCollisionQueryParams = InCollisionQueryParams;
	BackwardsCollisionQueryParams.bFindInitialOverlaps = false;

	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	PenetrationSceneCast(InWorld, ExitHitResults, BackwardsStart, InStart, InRotation, InTraceChannel, InCollisionShape, BackwardsCollisionQueryParams);

	MakeBackwardsHitsDataRelativeToForwadsSceneCast(ExitHitResults, EntranceHitResults);


	const FVector ForwardsDir = (InEnd - InStart).GetSafeNormal();
	OrderHitResultsInForwardsDirection(OutHits, EntranceHitResults, ExitHitResults, ForwardsDir);

	if (!ImpenetrableHit)
	{
		return nullptr;
	}
	return &OutHits.Last();
}
FExitAwareHitResult* UBFL_CollisionQueryHelpers::PenetrationLineTraceWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength, const bool bDrawDebugForBackwardsStart)
{
	FCollisionShape LineShape = FCollisionShape();
	return PenetrationSceneCastWithExitHits(InWorld, OutHits, InTraceStart, InTraceEnd, FQuat::Identity, InTraceChannel, LineShape, InCollisionQueryParams, IsHitImpenetrable, bOptimizeBackwardsSceneCastLength, bDrawDebugForBackwardsStart);
}
FExitAwareHitResult* UBFL_CollisionQueryHelpers::PenetrationSweepWithExitHits(const UWorld* InWorld, TArray<FExitAwareHitResult>& OutHits, const FVector& InSweepStart, const FVector& InSweepEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const TFunctionRef<bool(const FHitResult&)>& IsHitImpenetrable, const bool bOptimizeBackwardsSceneCastLength, const bool bDrawDebugForBackwardsStart)
{
	UE_CLOG(InCollisionShape.IsLine(), LogCollisionQueryHelpers, Warning, TEXT("%s() was used with a FCollisionShape::LineShape. Use the linetrace version if you want a line traces."), ANSI_TO_TCHAR(__FUNCTION__));
	return PenetrationSceneCastWithExitHits(InWorld, OutHits, InSweepStart, InSweepEnd, InRotation, InTraceChannel, InCollisionShape, InCollisionQueryParams, IsHitImpenetrable, bOptimizeBackwardsSceneCastLength, bDrawDebugForBackwardsStart);
}
//  END Custom query


//  BEGIN private functions
bool UBFL_CollisionQueryHelpers::SceneCastMultiByChannel(const UWorld* InWorld, TArray<FHitResult>& OutHits, const FVector& InStart, const FVector& InEnd, const FQuat& InRotation, const ECollisionChannel InTraceChannel, const FCollisionShape& InCollisionShape, const FCollisionQueryParams& InCollisionQueryParams, const FCollisionResponseParams& InCollisionResponseParams)
{
	// UWorld has SweepMultiByChannel() which already checks for zero extent shapes, but it doesn't explicitly check for ECollisionChannel::LineShape and its name can lead you to think that it doesn't support line traces
	if (InCollisionShape.IsLine())
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

FVector UBFL_CollisionQueryHelpers::DetermineBackwardsSceneCastStart(const TArray<FHitResult>& InForwardsHitResults, const FVector& InForwardsStart, const FVector& InForwardsEnd, const FHitResult* InHitStoppedAt, const bool bOptimizeBackwardsSceneCastLength, const float SweepShapeBoundingSphereRadius)
{
	const FVector ForwardDir = (InForwardsEnd - InForwardsStart).GetSafeNormal();
	
	if (InHitStoppedAt)
	{
		// We hit an impenetrable hit, so we don't want to start the backwards scene cast past that hit's location
		FVector BackwardsSceneCastStart = InHitStoppedAt->Location;

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

void UBFL_CollisionQueryHelpers::MakeBackwardsHitsDataRelativeToForwadsSceneCast(TArray<FHitResult>& InOutBackwardsHitResults, const TArray<FHitResult>& InForwardsHitResults)
{
	if (InOutBackwardsHitResults.Num() > 0)
	{
		const FHitResult& AForwardHit = InForwardsHitResults.Last();

		const float ForwardsSceneCastLength = UBFL_HitResultHelpers::CheapCalculateTraceLength(AForwardHit);
		const float BackwardsSceneCastLength = UBFL_HitResultHelpers::CheapCalculateTraceLength(InOutBackwardsHitResults.Last());

		for (FHitResult& HitResult : InOutBackwardsHitResults)
		{
			// Switch TraceStart and TraceEnd
			UBFL_HitResultHelpers::AdjustTraceDataBySlidingTraceStartAndEndByTime(HitResult, 1, 0);

			// Now make the hit's trace length the same as the forwards scene cast length
			// The backwards length is different when the optimization shortened it OR if we hit a stopping hit and it got shortened by wall avoidance padding
			const float TimeAtNewTraceEnd = ForwardsSceneCastLength / BackwardsSceneCastLength;
			UBFL_HitResultHelpers::AdjustTraceDataBySlidingTraceStartAndEndByTime(HitResult, 0, TimeAtNewTraceEnd);

			// Although we just calculated TraceStart and TraceEnd, our forwards hits already have TraceStart and TraceEnd without any floating point errors. May as well use them.
			HitResult.TraceStart = AForwardHit.TraceStart;
			HitResult.TraceEnd = AForwardHit.TraceEnd;
		}
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

void UBFL_CollisionQueryHelpers::DrawDebugForBackwardsStart(const UWorld* InWorld, const FCollisionShape& InCollisionShape, const FQuat& InRotation, const FVector& InBackwardsStart, const FVector& InBackwardsDir)
{
#if ENABLE_DRAW_DEBUG
	const FColor DebugColor = FColor::Cyan;
	const float DebugLifetime = 20.f;

	if (InCollisionShape.IsLine() == false)
	{
		// Draw scene cast shape
		UBFL_DrawDebugHelpers::DrawDebugCollisionShape(InWorld, InBackwardsStart, InCollisionShape, InRotation, DebugColor, 16, false, DebugLifetime, 0, 1.f);
	}

	// Draw backwards arrow
	DrawDebugLine(InWorld, InBackwardsStart, InBackwardsStart + (InBackwardsDir * 20.f), DebugColor, false, DebugLifetime, 0, 1.f);
	DrawDebugCone(InWorld, InBackwardsStart + (InBackwardsDir * 20.f), -InBackwardsDir, 10.f, FMath::DegreesToRadians(10.f), FMath::DegreesToRadians(10.f), 4, DebugColor, false, DebugLifetime, 0, 1.f);
#endif // ENABLE_DRAW_DEBUG
}
//  END private functions
