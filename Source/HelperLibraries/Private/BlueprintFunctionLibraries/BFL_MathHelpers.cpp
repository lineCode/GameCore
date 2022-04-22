// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_MathHelpers.h"



bool UBFL_MathHelpers::PointLiesOnSegment(const FVector& SegmentStart, const FVector& SegmentEnd, const FVector& Point)
{
    const float SegmentDistance = FVector::Distance(SegmentStart, SegmentEnd);
    const float SegmentStartToPointDistance = FVector::Distance(SegmentStart, Point);
    const float PointToSegmentEndDistance = FVector::Distance(Point, SegmentEnd);

    // If SegmentEnd, SegmentEnd, and Point do not form a triangle, then Point is on the segment
    if (FMath::IsNearlyEqual(SegmentStartToPointDistance + PointToSegmentEndDistance, SegmentDistance))
    {
        return true;
    }

    // Point is not on the segment
    return false;
}
