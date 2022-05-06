// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"



bool UBFL_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameEnd = (HitA.TraceEnd == HitB.TraceEnd);

	const bool bSameTrace = (bSameStart && bSameEnd);
	return bSameTrace;
}
