	// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/HLBlueprintFunctionLibrary_HitResultHelpers.h"



bool UHLBlueprintFunctionLibrary_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameTraceStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameTraceEnd = (HitA.TraceEnd == HitB.TraceEnd);


	const bool bSameTrace = (bSameTraceStart && bSameTraceEnd);
	return bSameTrace;
}

float UHLBlueprintFunctionLibrary_HitResultHelpers::CheapCalculateTraceLength(const FHitResult& InHit)
{
	if (InHit.TraceStart == InHit.TraceEnd)
	{
		return 0.f;
	}

	if (InHit.Time == 0.f)
	{
		UE_LOG(LogHLHitResultHelpers, Verbose, TEXT("%s() Cannot cheaply calculate trace length from a hit result with time of 0. Fall back on normal (more expensive) method for calculating"), ANSI_TO_TCHAR(__FUNCTION__));
		return FVector::Distance(InHit.TraceStart, InHit.TraceEnd);
	}
	
	// Return the hit's distance scaled up by the trace time
	return InHit.Distance * (1 / InHit.Time);
}

void UHLBlueprintFunctionLibrary_HitResultHelpers::AdjustTraceDataBySlidingTraceStartAndEndByTime(FHitResult& InOutHit, const float InTimeAtNewTraceStart, const float InTimeAtNewTraceEnd)
{
	const float OriginalTraceLength = CheapCalculateTraceLength(InOutHit);

	// Set new trace points
	{
		const float TraceStartAdjustment = InTimeAtNewTraceStart * OriginalTraceLength;
		const FVector ForwardsTraceDirection = (InOutHit.TraceEnd - InOutHit.TraceStart).GetSafeNormal();
		InOutHit.TraceStart = InOutHit.TraceStart + (ForwardsTraceDirection * TraceStartAdjustment);

		const float TraceEndAdjustment = (1 - InTimeAtNewTraceEnd) * OriginalTraceLength;
		const FVector BackwardsTraceDirection = -ForwardsTraceDirection;
		InOutHit.TraceEnd = InOutHit.TraceEnd + (BackwardsTraceDirection * TraceEndAdjustment);
	}

	const float TotalTimeMultiplier = (InTimeAtNewTraceEnd - InTimeAtNewTraceStart);
	const float NewTraceLength = FMath::Abs(OriginalTraceLength * TotalTimeMultiplier);

	// Update trace-related data
	{
		InOutHit.Time -= InTimeAtNewTraceStart; // move our hit time with the new start time
		InOutHit.Time /= TotalTimeMultiplier;   // then adjust it with the total multiplier

		InOutHit.Distance = NewTraceLength * InOutHit.Time;
	}


	if (InOutHit.Time < 0.f || InOutHit.Time > 1.f)
	{
		UE_LOG(LogHLHitResultHelpers, Error, TEXT("%s(): The new TraceStart and TraceEnd has put the hit out of range of the trace"), ANSI_TO_TCHAR(__FUNCTION__));
		check(0);
	}
}
