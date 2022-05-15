// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_ShooterHelpers.h"

#include "BlueprintFunctionLibraries/BFL_CollisionQueryHelpers.h"

#include "DrawDebugHelpers.h"



float UBFL_ShooterHelpers::NerfSpeedPerCm(float& InOutSpeed, const float InDistanceToTravel, const float InNerfPerCm)
{
	const float SpeedToTakeAway = (InNerfPerCm * InDistanceToTravel);
	const float TraveledThroughRatio = (InOutSpeed / SpeedToTakeAway);
	const float TraveledThroughDistance = (TraveledThroughRatio * InDistanceToTravel);
	InOutSpeed -= SpeedToTakeAway;

	return TraveledThroughDistance;
}

void FScanResult::DebugScan(const UWorld* InWorld) const
{
#if ENABLE_DRAW_DEBUG
	const float DebugLifeTime = 5.f;
	const FColor TraceColor = FColor::Blue;
	const FColor HitColor = FColor::Red;

	for (const FHitResult& Hit : BulletHits)
	{
		DrawDebugLine(InWorld, Hit.TraceStart, Hit.Location, TraceColor, false, DebugLifeTime);
		DrawDebugPoint(InWorld, Hit.ImpactPoint, 10.f, HitColor, false, DebugLifeTime);
	}

	if (BulletHits.Num() > 0)
	{
		DrawDebugLine(InWorld, BulletHits.Last().Location, BulletEnd, TraceColor, false, DebugLifeTime);
	}
	else
	{
		DrawDebugLine(InWorld, BulletStart, BulletEnd, TraceColor, false, DebugLifeTime);
	}
#endif // ENABLE_DRAW_DEBUG
}
