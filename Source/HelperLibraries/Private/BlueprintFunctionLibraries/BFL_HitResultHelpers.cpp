// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"



bool UBFL_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameTraceStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameTraceEnd = (HitA.TraceEnd == HitB.TraceEnd);


	const bool bSameTrace = (bSameTraceStart && bSameTraceEnd);
	return bSameTrace;
}

float UBFL_HitResultHelpers::GetTraceLengthFromHit(const FHitResult& Hit, const bool bEnsureThatDistanceIsNotCalculated)
{
	if (Hit.TraceStart == Hit.TraceEnd)
	{
		return 0.f;
	}

	if (Hit.Time == 0.f)
	{
		if (bEnsureThatDistanceIsNotCalculated)
		{
			UE_LOG(LogHitResultHelpers, Warning, TEXT("%s() Cannot extract trace length from hit result with time of 0"), ANSI_TO_TCHAR(__FUNCTION__));
			check(0);
			return -1.f;
		}

		return FVector::Distance(Hit.TraceStart, Hit.TraceEnd);
	}
	
	// Return the hit's distance scaled up by the trace time
	return Hit.Distance * (1 / Hit.Time);
}
