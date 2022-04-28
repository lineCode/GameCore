// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"

#include "Utilities/HLLogCategories.h"

bool UBFL_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameEnd = (HitA.TraceEnd == HitB.TraceEnd);
	//const bool bSamePenetration = (HitA.bStartPenetrating == HitB.bStartPenetrating);

	const bool bSameTrace = (bSameStart && bSameEnd/* && bSamePenetration*/);
	return bSameTrace;
}
