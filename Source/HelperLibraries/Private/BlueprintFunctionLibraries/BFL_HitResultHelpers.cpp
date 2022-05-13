// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"



bool UBFL_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameTraceStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameTraceEnd = (HitA.TraceEnd == HitB.TraceEnd);
	const bool bSameTraceLength = (GetTraceLengthFromHit(HitA) == GetTraceLengthFromHit(HitB));


	const bool bSameTrace = (bSameTraceStart && bSameTraceEnd && bSameTraceLength);
	return bSameTrace;
}

float UBFL_HitResultHelpers::GetTraceLengthFromHit(const FHitResult& Hit)
{
	return Hit.Distance * (1 / Hit.Time); // return the hit's distance scaled up by the trace time
}
