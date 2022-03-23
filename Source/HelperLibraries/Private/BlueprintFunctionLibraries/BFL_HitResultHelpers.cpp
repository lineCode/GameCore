// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_HitResultHelpers.h"

#include "Utilities/HLLogCategories.h"



UMaterialInterface* UBFL_HitResultHelpers::GetHitMaterial(const FHitResult& InHitResult)
{
	UMaterialInterface* RetVal = nullptr;
	if (InHitResult.FaceIndex == -1)
	{
		UE_LOG(LogHitResultHelpers, Error, TEXT("%s() failed because InHitResult.FaceIndex was -1. Make sure FCollisionQueryParams::bTraceComplex and FCollisionQueryParams::bReturnFaceIndex are set to true"), ANSI_TO_TCHAR(__FUNCTION__));
		return RetVal;
	}


	const UPrimitiveComponent* HitComponent = InHitResult.GetComponent();
	if (IsValid(HitComponent))
	{
		int32 SectionIndex;
		RetVal = HitComponent->GetMaterialFromCollisionFaceIndex(InHitResult.FaceIndex, SectionIndex);
	}


	return RetVal;
}

void UBFL_HitResultHelpers::GetSectionLevelHitInfo(const FHitResult& InHitResult, OUT UPrimitiveComponent*& OutHitPrimitiveComponent, int32& OutHitSectionIndex)
{
	OutHitPrimitiveComponent = nullptr;
	OutHitSectionIndex = -1;


	if (InHitResult.FaceIndex == -1)
	{
		UE_LOG(LogHitResultHelpers, Error, TEXT("%s() failed because InHitResult.FaceIndex was -1. Make sure FCollisionQueryParams::bTraceComplex and FCollisionQueryParams::bReturnFaceIndex are set to true"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
	OutHitPrimitiveComponent = InHitResult.GetComponent();
	if (!IsValid(OutHitPrimitiveComponent))
	{
		UE_LOG(LogHitResultHelpers, Error, TEXT("%s() failed because InHitResult.HitComponent was NULL. We expected something of type UPrimitiveComponent to be hit, but doesn't look like any primitive component was hit. Either that or we were provided with a bad pointer somehow (unlikely)."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}



	OutHitPrimitiveComponent->GetMaterialFromCollisionFaceIndex(InHitResult.FaceIndex, OutHitSectionIndex);
}

bool UBFL_HitResultHelpers::AreHitsFromSameTrace(const FHitResult& HitA, const FHitResult& HitB)
{
	const bool bSameStart = (HitA.TraceStart == HitB.TraceStart);
	const bool bSameEnd = (HitA.TraceEnd == HitB.TraceEnd);
	//const bool bSamePenetration = (HitA.bStartPenetrating == HitB.bStartPenetrating);

	const bool bSameTrace = (bSameStart && bSameEnd/* && bSamePenetration*/);
	return bSameTrace;
}
