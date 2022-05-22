// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"



bool UBFL_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameTraceStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameTraceEnd = (HitA.TraceEnd == HitB.TraceEnd);


	const bool bSameTrace = (bSameTraceStart && bSameTraceEnd);
	return bSameTrace;
}

float UBFL_HitResultHelpers::CheapCalculateTraceLength(const FHitResult& InHit)
{
	if (InHit.TraceStart == InHit.TraceEnd)
	{
		return 0.f;
	}

	if (InHit.Time == 0.f)
	{
		UE_LOG(LogHitResultHelpers, Verbose, TEXT("%s() Cannot cheaply calculate trace length from a hit result with time of 0. Fall back on normal (more expensive) method for calculating"), ANSI_TO_TCHAR(__FUNCTION__));
		return FVector::Distance(InHit.TraceStart, InHit.TraceEnd);
	}
	
	// Return the hit's distance scaled up by the trace time
	return InHit.Distance * (1 / InHit.Time);
}
