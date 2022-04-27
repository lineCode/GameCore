// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "Utilities/HLLogCategories.h"
#include "Kismet/KismetMathLibrary.h"
#include "Algo/Reverse.h"



bool UBFL_CollisionQueryHelpers::LineTraceMultiByChannelWithPenetrations(const UWorld* InWorld, TArray<FHitResult>& OutHitResults, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& ShouldNotPenetrate)
{
	// Ensure our collision query params do NOT ignore overlaps because we are tracing as an ECR_Overlap (otherwise we won't get any Hit Results)
	FCollisionQueryParams TraceCollisionQueryParams = InCollisionQueryParams;
	TraceCollisionQueryParams.bIgnoreTouches = false;

	// Perform the trace
	// Use ECR_Overlap to have this trace overlap through everything
	// Also hardcode ECC_Visibility as our trace channel because it doesn't affect anything
	InWorld->LineTraceMultiByChannel(OutHitResults, InTraceStart, InTraceEnd, ECollisionChannel::ECC_Visibility, TraceCollisionQueryParams, FCollisionResponseParams(ECollisionResponse::ECR_Overlap));


	for (int32 i = 0; i < OutHitResults.Num(); ++i)
	{
		// Emulate the use of a trace channel by manually setting FHitResult::bBlockingHit and removing any hits that are ignored by the InTraceChannel
		if (const UPrimitiveComponent* PrimitiveComponent = OutHitResults[i].Component.Get())
		{
			ECollisionChannel ComponentCollisionChannel;
			FCollisionResponseParams ComponentResponseParams;
			UCollisionProfile::GetChannelAndResponseParams(PrimitiveComponent->GetCollisionProfileName(), ComponentCollisionChannel, ComponentResponseParams);

			// The hit component's response to our InTraceChannel
			const ECollisionResponse CollisionResponse = ComponentResponseParams.CollisionResponse.GetResponse(InTraceChannel);


			if (CollisionResponse == ECollisionResponse::ECR_Ignore)
			{
				// This hit component is ignored by our InTraceChannel

				// Ignore this hit
				OutHitResults.RemoveAt(i);
				--i;
				continue;
			}
			else if (CollisionResponse == ECollisionResponse::ECR_Overlap)
			{
				// This hit component overlaps our InTraceChannel
				OutHitResults[i].bBlockingHit = false;

				if (InCollisionQueryParams.bIgnoreTouches)
				{
					// Ignore touch
					OutHitResults.RemoveAt(i);
					--i;
					continue;
				}
			}
			else if (CollisionResponse == ECollisionResponse::ECR_Block)
			{
				// This hit component blocks our InTraceChannel
				OutHitResults[i].bBlockingHit = true;

				if (InCollisionQueryParams.bIgnoreBlocks)
				{
					// Ignore block
					OutHitResults.RemoveAt(i);
					--i;
					continue;
				}
			}
		}

		UE_CLOG((OutHitResults[i].Distance == 0.f), LogCollisionQueryHelpers, Warning, TEXT("%s() OutHitResults[%d] has a distance of 0! This could be due to starting the trace inside of geometry when using simple collision"), ANSI_TO_TCHAR(__FUNCTION__), i);

		if (ShouldNotPenetrate != nullptr && ShouldNotPenetrate(OutHitResults[i])) // run caller's function to see if they want this hit result to be the last one
		{
			// Remove the rest if there are any
			if (OutHitResults.IsValidIndex(i + 1))
			{
				OutHitResults.RemoveAt(i + 1, (OutHitResults.Num() - 1) - i);
			}

			return true;
		}
	}

	// No impenetrable hits to stop us
	return false;
}
bool UBFL_CollisionQueryHelpers::DoubleSidedLineTraceMultiByChannelWithPenetrations(const UWorld* InWorld, TArray<FHitResult>& OutHitResults, const FVector& InTraceStart, const FVector& InTraceEnd, const ECollisionChannel InTraceChannel, const FCollisionQueryParams& InCollisionQueryParams, const TFunction<bool(const FHitResult&)>& ShouldNotPenetrate)
{
	TArray<FHitResult> EntranceHitResults;
	const bool bHitImpenetrableHit = LineTraceMultiByChannelWithPenetrations(InWorld, EntranceHitResults, InTraceStart, InTraceEnd, InTraceChannel, InCollisionQueryParams, ShouldNotPenetrate);
	if (EntranceHitResults.Num() <= 0)
	{
		return bHitImpenetrableHit; // nothing to work with. Also this will always returns false
	}


	FVector BackwardsTraceStart;
	const FVector ForwardsTraceDir = (InTraceEnd - InTraceStart).GetSafeNormal();

	// Good number for ensuring we don't start a trace on top of the object
	const float TraceStartWallAvoidancePadding = .01f;
	if (bHitImpenetrableHit)
	{
		// We hit an impenetrable hit, so we don't want to start the backwards trace past that hit's location
		BackwardsTraceStart = EntranceHitResults.Last().Location;

		// Bump us away from the hit location a little so that the backwards trace doesn't get stuck
		BackwardsTraceStart += ((ForwardsTraceDir * -1) * TraceStartWallAvoidancePadding);
	}
	else
	{
		// Start backwards trace at the end of the forwards trace, BUT there is actually an optimization we can do instead
		// We use the bounding sphere of each hit object and compare their diameters to get the largest one. Then we add this distance with our last hit location along the fwd trace direction so that we can mimimize the trace length by finding the furthest possible point that goes passed all of our hit components
		// This optimization is good, BUT there is 1 case that this doesn't cover and that is if the forwards trace starts from within an object and it leaves that object passed our optimized backwards trace start location (this is absurdly unlikely however)

		// Get the largest bounding diameter out of all the hit components
		float LargestHitComponentBoundingDiameter = 0.f;
		for (const FHitResult& HitResult : EntranceHitResults)
		{
			if (HitResult.Component.IsValid())
			{
				const float HitComponentBoundingDiameter = (HitResult.Component->Bounds.SphereRadius * 2);
				if (HitComponentBoundingDiameter > LargestHitComponentBoundingDiameter)
				{
					LargestHitComponentBoundingDiameter = HitComponentBoundingDiameter;
				}
			}
		}

		// Furthest possible exit point of the penetration
		BackwardsTraceStart = EntranceHitResults.Last().Location + (ForwardsTraceDir * (LargestHitComponentBoundingDiameter + TraceStartWallAvoidancePadding));	// we use TraceStartWallAvoidancePadding here not to ensure that we don't hit the wall (b/c we do want the trace to hit it), but to just ensure we don't start inside it to remove possibility for unpredictable results. Very unlikely we hit a case where this padding is actually helpful here, but doing it to cover all cases.
	}


	TArray<FHitResult> ExitHitResults;
	ExitHitResults.Reserve(EntranceHitResults.Num());
	LineTraceMultiByChannelWithPenetrations(InWorld, ExitHitResults, BackwardsTraceStart, InTraceStart, InTraceChannel, InCollisionQueryParams, ShouldNotPenetrate);

	// Fill out OutHitResults with both entrance and exit Hit Results
	OutHitResults.Reserve(EntranceHitResults.Num() + ExitHitResults.Num());
	for (const FHitResult& EntranceHitResult : EntranceHitResults)
	{
		for (int32 i = ExitHitResults.Num() - 1; i >= 0; --i) // iterate backwards because exits were traced in the opposite direction
		{
			const FHitResult& ExitHitResult = ExitHitResults[i];

			const bool EntranceHitIsFirst = FVector::DotProduct(ForwardsTraceDir, (ExitHitResult.Location - EntranceHitResult.Location)) > 0;
			if (EntranceHitIsFirst)
			{
				// Break out of here to add the entrance hit first
				break;
			}
			else // this exit is first
			{
				OutHitResults.Add(ExitHitResult);
				ExitHitResults.RemoveAt(i); // make sure we don't come across this exit again
				continue;
			}
		}

		OutHitResults.Add(EntranceHitResult);
	}

	if (ExitHitResults.Num() > 0)
	{
		// Since we have added all of our entrance hits, we know that the rest of the exit hits come after them.
		// Add the rest of the exit hits.
		for (int32 i = ExitHitResults.Num() - 1; i >= 0; --i)
		{
			OutHitResults.Add(ExitHitResults[i]);
		}
	}

	return bHitImpenetrableHit;
}



void UBFL_CollisionQueryHelpers::BuildTracePhysMatStackPoints(OUT TArray<FTracePhysMatStackPoint>& OutTracePhysMatStackPoints, const TArray<FHitResult>& FwdBlockingHits, const UWorld* World, const FCollisionQueryParams& TraceParams, const TEnumAsByte<ECollisionChannel> TraceChannel)
{

}
void UBFL_CollisionQueryHelpers::BuildTracePhysMatStackPoints(OUT TArray<FTracePhysMatStackPoint>& OutTracePhysMatStackPoints, const TArray<FHitResult>& FwdBlockingHits, const FVector& FwdEndLocation, const UWorld* World, const FCollisionQueryParams& TraceParams, const TEnumAsByte<ECollisionChannel> TraceChannel)
{

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
