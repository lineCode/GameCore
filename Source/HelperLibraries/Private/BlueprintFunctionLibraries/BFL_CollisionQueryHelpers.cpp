// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "Utilities/HLLogCategories.h"
#include "Kismet/KismetMathLibrary.h"
#include "Algo/Reverse.h"



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


void UBFL_CollisionQueryHelpers::BuildTraceSegments(OUT TArray<FTraceSegment>& OutTraceSegments, const TArray<FHitResult>& FwdBlockingHits, const UWorld* World, const FCollisionQueryParams& TraceParams, const TEnumAsByte<ECollisionChannel> TraceChannel)
{
	if (FwdBlockingHits.Num() <= 0)
	{
		UE_LOG(LogCollisionQueryHelpers, Warning, TEXT("%s(): Wasn't given any FwdBlockingHits to build any Trace Segments of. Returned and did nothing"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}


	// This is the furthest possible FwdEndLocation for the given FwdBlockingHits
	FVector FurthestPossibleEnd;

	// Calculate the FurthestPossibleEnd point out of these FwdBlockingHits
	{
		const FVector TracedDir = UKismetMathLibrary::GetDirectionUnitVector(FwdBlockingHits[0].TraceStart, FwdBlockingHits[0].TraceEnd);
		
		float CurrentFurthestPossibleSize = 0.f;
		for (const FHitResult& Hit : FwdBlockingHits)
		{
			if (Hit.Component.IsValid() == false)
			{
				continue;
			}

			// The Impact Point plus the bounding diameter
			const FVector ThisHitFurthestPossibleEnd = Hit.ImpactPoint + (TracedDir * (Hit.Component.Get()->Bounds.SphereRadius * 2));

			// If this ThisHitFurthestPossibleEnd is further than our current FurthestPossibleEnd then update FurthestPossibleEnd
			const float thisHitFurthestPossibleSize = (ThisHitFurthestPossibleEnd - FwdBlockingHits[0].TraceStart).SizeSquared();
			if (thisHitFurthestPossibleSize > CurrentFurthestPossibleSize)
			{
				FurthestPossibleEnd = ThisHitFurthestPossibleEnd;
				CurrentFurthestPossibleSize = thisHitFurthestPossibleSize;
			}

		}

		// Bump out a little because the BuildTraceSegments() will bump us in a little
		FurthestPossibleEnd += (TracedDir * ((KINDA_SMALL_NUMBER * 100) * 2));
	}

	// Use this FurthestPossibleEnd as a Bkwd trace start location
	BuildTraceSegments(OutTraceSegments, FwdBlockingHits, FurthestPossibleEnd, World, TraceParams, TraceChannel);
}

void UBFL_CollisionQueryHelpers::BuildTraceSegments(OUT TArray<FTraceSegment>& OutTraceSegments, const TArray<FHitResult>& FwdBlockingHits, const FVector& FwdEndLocation, const UWorld* World, const FCollisionQueryParams& TraceParams, const TEnumAsByte<ECollisionChannel> TraceChannel)
{
	// 
	// 						GENERAL GOAL
	// 
	// 
	// 		OutTraceSegments[0]		OutTraceSegments[1]
	// 			|-------|				|-------|
	// 
	// 
	// 			_________				_________
	// 			|		|				|		|
	// ---------O-------|---------------O-------|------------------> // forward traces
	// 			|		|				|		|
	// <--------|-------O---------------|-------O------------------- // backwards traces
	// 			|		|				|		|
	// 			|		|				|		|
	// 			|		|				|		|
	// 			|		|				|		|
	// 			_________				_________
	// 
	// We can simplify this concept by visualizing it in 2d
	// 

	if (FwdBlockingHits.Num() <= 0)
	{
		UE_LOG(LogCollisionQueryHelpers, Warning, TEXT("%s(): Wasn't given any FwdBlockingHits to build any Trace Segments of. Returned and did nothing"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	OutTraceSegments.Empty(FwdBlockingHits.Num() * 2); // we know, most of the time, that we will have at least double the elements from FwdBlockingHits (most of the time) so reserve this much


	const FVector FwdStartLocation = FwdBlockingHits[0].TraceStart; // maybe make this a parameter since FwdEndLocation is one


	TArray<UPhysicalMaterial*> CurrentEntrancePhysMaterials;		// These don't point to a specific instance of a phys material in the game (not possible with phys mats). They just point to the type.

	FCollisionQueryParams BkwdParams = TraceParams;
	BkwdParams.bIgnoreTouches = true; // we only care about blocking hits

		
	for (int32 i = -1; i < FwdBlockingHits.Num(); ++i)
	{
		//const FHitResult& ThisFwdBlockingHit = FwdBlockingHits[i];

		FVector BkwdStart;
		if (FwdBlockingHits.IsValidIndex(i + 1))
		{
			BkwdStart = FwdBlockingHits[i + 1].Location;
		}
		else
		{
			BkwdStart = FwdEndLocation;
		}
		FVector BkwdEnd;
		if (i == -1)
		{
			BkwdEnd = FwdStartLocation;
		}
		else
		{
			BkwdEnd = FwdBlockingHits[i].ImpactPoint; // NOTE: this would be weird for collision sweeps
		}



		if (i != -1)
		{
			// This stack tells us how deep we are and provides us with the Phys Materials of the entrance hits
			CurrentEntrancePhysMaterials.Push(FwdBlockingHits[i].PhysMaterial.Get());
		}


		TArray<FHitResult> BkwdHitResults;
		LineTracePenetrateBetweenPoints(BkwdHitResults, World, BkwdStart, BkwdEnd, TraceChannel, BkwdParams);
		Algo::Reverse<TArray<FHitResult>>(BkwdHitResults);


		// Build the Segment from this FwdBlockingHit to the BkwdHitResult (connecting a Fwd hit to a Bkwd hit)
		{
			const FVector StartPoint = (i == -1) ? BkwdEnd : FwdBlockingHits[i].ImpactPoint;
			const FVector EndPoint = (BkwdHitResults.Num() > 0) ? BkwdHitResults[0].ImpactPoint : BkwdStart;
			FTraceSegment Segment = FTraceSegment(StartPoint, EndPoint); // the Segment we will make for this distance we just traced
			Segment.PhysMaterials = CurrentEntrancePhysMaterials;

			if (FwdBlockingHits.IsValidIndex(i + 1) && FMath::IsNearlyZero(Segment.GetSegmentDistance()) == false) // if we are the last Segment and our distance is zero don't add this
			{
				OutTraceSegments.Add(Segment);
			}
		}

		// Build the rest of the BkwdHitResults Segments (connecting Bkwd hits to Bkwd hits)
		for (const FHitResult& BkwdHit : BkwdHitResults)
		{

			// Remove this Phys Mat from the Phys Mat stack because we are exiting it
			UPhysicalMaterial* PhysMatThatWeAreExiting = BkwdHit.PhysMaterial.Get();
			int32 IndexOfPhysMatThatWeAreExiting = CurrentEntrancePhysMaterials.FindLast(PhysMatThatWeAreExiting); // the inner-most (last) occurrence of this Phys Mat is the one that we are exiting
			if (IndexOfPhysMatThatWeAreExiting != INDEX_NONE)
			{
				CurrentEntrancePhysMaterials.RemoveAt(IndexOfPhysMatThatWeAreExiting); // remove this Phys Mat that we are exiting from the stack
			}
			else
			{
				// We've just realized that we have been inside of something from the start (because we just exited something that we've never entered).
				// Add this something to all of the Segments we have made so far
				for (FTraceSegment& TraceSegmentToAddTo : OutTraceSegments)
				{
					// Insert this newly discovered Phys Mat at the bottom of the stack because it was here the whole time
					TraceSegmentToAddTo.PhysMaterials.Insert(PhysMatThatWeAreExiting, 0);
				}
			}

			// Add a Segment for this distance that we just traced
			FTraceSegment Segment = FTraceSegment(BkwdHit.ImpactPoint, BkwdHit.TraceStart);
			Segment.PhysMaterials = CurrentEntrancePhysMaterials;

			OutTraceSegments.Add(Segment);
		}


	}

}

void UBFL_CollisionQueryHelpers::LineTracePenetrateBetweenPoints(OUT TArray<FHitResult>& OutHitResults, const UWorld* World, const FVector& Start, const FVector& End, const ECollisionChannel TraceChannel, const FCollisionQueryParams& Params)
{
	if (Start.Equals(End, KINDA_SMALL_NUMBER + (KINDA_SMALL_NUMBER * 100)))
	{
		return;		// The given Start and End locations were nearly the same which means there is nothing to do. If we continued it would mess things up (infinite trace loop). Returned early and did nothing.
	}



	OutHitResults.Empty();




	FCollisionQueryParams PenParams = Params;
	PenParams.bTraceComplex = true; // Use complex collision if the collider has it. We need this because we are starting from inside the geometry and shooting out (this won't work for CTF_UseSimpleAsComplex and Physics Assest colliders so TODO: we will need a case that covers this)


	// Get trace dir from this hit
	const FVector FwdDir = UKismetMathLibrary::GetDirectionUnitVector(Start, End);
	const FVector BkwdDir = -1 * FwdDir; // get the opposite direction


	FVector PenStart = Start + ((KINDA_SMALL_NUMBER * 100) * FwdDir);
	FVector PenEnd = End + ((KINDA_SMALL_NUMBER * 100) * BkwdDir);


	// Penetrate traces loop
	for (FHitResult PenHitResult; World->LineTraceSingleByChannel(PenHitResult, PenStart, PenEnd, TraceChannel, PenParams); PenStart = PenHitResult.Location + ((KINDA_SMALL_NUMBER * 100) * FwdDir))
	{
		if (PenHitResult.Distance == 0)	// If the trace messed up
		{
			// Unsuccessful backwards trace (we didn't travel anywhere but hit something)
			// Likely reason: It's stuck inside a collider that's using simple collision


			// Fallback method...........
			// Try again with this component (collider) ignored so we can move on
			const int32 AmtOfFallbackTracesToTry = 5;
			if (AmtOfFallbackTracesToTry > 0)
			{
				bool bFallbackMethodReachedBkwdEnd = false;
				FCollisionQueryParams FallbackQueryParams = Params;
				for (int32 j = 0; j < AmtOfFallbackTracesToTry; j++)
				{
					FallbackQueryParams.AddIgnoredComponent(PenHitResult.GetComponent());	// ignore our last trace's blocking hit
					if (!World->LineTraceSingleByChannel(PenHitResult, PenStart, PenEnd, TraceChannel, FallbackQueryParams))
					{
						//	We want to stop doing fallback traces since we are at the end of our bkwd trace now (the beginning of our fwd traces)
						//	We will also tell the outer loop to break as well by setting this bool
						bFallbackMethodReachedBkwdEnd = true;
						break;
					}
					// If we did hit something.....

					if (PenHitResult.Distance == 0)	// if the distance is still 0, we will keep trying again until loop ends
					{
						continue;
					}
					else
					{
						break;
					}
				}


				if (bFallbackMethodReachedBkwdEnd)
				{
					break;		// Fallback method broke the loop since it reached BkwdEnd
				}
			}
		}





		OutHitResults.Add(PenHitResult);


	}

}
