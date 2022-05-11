// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_MathHelpers.h"



float UBFL_MathHelpers::GetCollisionShapeBoundingSphereRadius(const FCollisionShape& CollisionShape)
{
	switch (CollisionShape.ShapeType)
	{
		case ECollisionShape::Box:
		{
			const FVector BoxExtent = FVector(CollisionShape.Box.HalfExtentX * 2, CollisionShape.Box.HalfExtentY * 2, CollisionShape.Box.HalfExtentZ * 2);
			return GetBoxBoundingSphereRadius(BoxExtent);
		}
		case  ECollisionShape::Sphere:
		{
			return CollisionShape.Sphere.Radius;
		}
		case ECollisionShape::Capsule:
		{
			return CollisionShape.Capsule.HalfHeight;
		}
	}

	return 0.f;
}

float UBFL_MathHelpers::GetBoxBoundingSphereRadius(const FVector& BoxExtent)
{
	const float LengthSquared = (BoxExtent.X * BoxExtent.X);
	const float WidthSquared  = (BoxExtent.Y * BoxExtent.Y);
	const float HeightSquared = (BoxExtent.Z * BoxExtent.Z);

	float Diameter = FMath::Sqrt(LengthSquared + WidthSquared + HeightSquared); // 3d pythagorean theorem

	// Or you can get the distance across opposite corners
	// float Diameter = FVector::Distance(FVector(0.f, 0.f, 0.f), BoxExtent)

	return Diameter / 2;
}

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

FVector UBFL_MathHelpers::GetLocationAimDirection(const UWorld* World, const FCollisionQueryParams& QueryParams, const FVector& AimPoint, const FVector& AimDir, const float& MaxRange, const FVector& Location)
{
	if (Location.Equals(AimPoint))
	{
		// The Location is the same as the AimPoint so we can skip the camera trace and just return the AimDir as the muzzle's direction
		return AimDir;
	}

	// Line trace from the Location to the point that AimDir is looking at
	FCollisionQueryParams CollisionQueryParams = QueryParams;
	CollisionQueryParams.bIgnoreTouches = true;

	FVector TraceEnd = AimPoint + (AimDir * MaxRange);

	FHitResult HitResult;
	const bool bSuccess = World->LineTraceSingleByChannel(HitResult, AimPoint, TraceEnd, ECollisionChannel::ECC_Visibility, CollisionQueryParams);

	if (!bSuccess)
	{
		// AimDir is not looking at anything for our Location to point to
		return (TraceEnd - Location).GetSafeNormal();
	}

	// Return the direction from the Location to the point that the Player is looking at
	return (HitResult.Location - Location).GetSafeNormal();
}
