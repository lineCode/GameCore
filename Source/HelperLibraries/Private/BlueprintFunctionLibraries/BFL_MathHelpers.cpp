// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_MathHelpers.h"



bool UBFL_MathHelpers::PointLiesOnSegment(const FVector& Start, const FVector& End, const FVector& Point)
{
    const float SegmentDistance = FVector::Distance(Start, End);
    const float StartToPointDistance = FVector::Distance(Start, Point);
    const float PointToEndDistance = FVector::Distance(Point, End);

    if (FMath::IsNearlyEqual(StartToPointDistance + PointToEndDistance, SegmentDistance)) // if we are not a triangle
    {
        return true;
    }
    return false;
}
