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
		case ECollisionShape::Line:
		{
			// A LineShape is just a point
			return 0.f;
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


bool UBFL_MathHelpers::PointLiesOnSegment(const FVector& InSegmentStart, const FVector& InSegmentEnd, const FVector& InPoint, const float InTolerance)
{
	if (PointsAreCollinear({ InSegmentStart, InSegmentEnd, InPoint }, InTolerance))
	{
		// The point is on the line that start and end is on

		const FVector StartToPointDirectionUnnormalized = (InPoint - InSegmentStart);
		const FVector PointToEndDirectionUnnormalized = (InSegmentEnd - InPoint);

		const FVector SegmentDirectionUnnormalized = (InSegmentEnd - InSegmentStart);
		const bool StartIsBeforePoint = (FVector::DotProduct(StartToPointDirectionUnnormalized, SegmentDirectionUnnormalized) >= 0);
		const bool PointIsBeforeEnd = (FVector::DotProduct(PointToEndDirectionUnnormalized, SegmentDirectionUnnormalized) >= 0);

		if (StartIsBeforePoint && PointIsBeforeEnd)
		{
			// The point is within start and end
			return true;
		}
	}

	// Point is not on the segment
	return false;
}

bool UBFL_MathHelpers::PointsAreCollinear(const TArray<FVector>& InPoints, const float InTolerance)
{
	if (InPoints.Num() <= 2)
	{
		// Two points are always collinear
		return true;
	}

	const FVector LineDirection = (InPoints[1] - InPoints[0]).GetSafeNormal();

	for (int32 i = 2; i < InPoints.Num(); ++i) // skip the first two points
	{
		const FVector DirectionToPoint = (InPoints[i] - InPoints[0]).GetSafeNormal();

		const bool bSameDirection = FMath::IsNearlyEqual(FVector::DotProduct(DirectionToPoint, LineDirection), 1, InTolerance);
		const bool bOppositeDirection = FMath::IsNearlyEqual(FVector::DotProduct(DirectionToPoint, LineDirection), -1, InTolerance);
		const bool bParallel = (bSameDirection || bOppositeDirection);
		if (!bParallel)
		{
			// Not collinear
			return false;
		}
	}

	// All points lie on the same line
	return true;
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
