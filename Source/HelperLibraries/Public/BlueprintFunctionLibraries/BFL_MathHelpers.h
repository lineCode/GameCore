// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BFL_MathHelpers.generated.h"



/**
 * A collection of common math helpers
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_MathHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	 * Gets the radius of a collision shape's bounding sphere
	 */
	static float GetCollisionShapeBoundingSphereRadius(const FCollisionShape& CollisionShape);

	/**
	 * Gets the radius of a box's bounding sphere
	 */
	UFUNCTION(BlueprintPure, Category = "MathHelpers|GeometryMath")
		static float GetBoxBoundingSphereRadius(const FVector& BoxExtent);


	/**
	 * Given a segment and a point, does that point lie on the segment?
	 * NOTE: Does 3 distance calculations (3 sqrts)
	 */
	UFUNCTION(BlueprintPure, Category = "MathHelpers|VectorMath")
		static bool PointLiesOnSegment(const FVector& Start, const FVector& End, const FVector& Point);

	/**
	 * Gets the direction from the Location to the point that AimDir is looking at.
	 * E.g. having a weapon's muzzle aim towards the Player's look point.
	 */
	static FVector GetLocationAimDirection(const UWorld* InWorld, const FCollisionQueryParams& Params, const FVector& AimPoint, const FVector& AimDir, const float& MaxRange, const FVector& Location);

	/** Performs linear interpolation on any number of values */
	template <class T>
	static T LerpMultiple(const TArray<T>& Values, const float InAlpha)
	{
		if (Values.Num() <= 0)
		{
			UE_LOG(LogMathHelpers, Warning, TEXT("%s() was not given any values to lerp between! Returning a default value."), ANSI_TO_TCHAR(__FUNCTION__));
			return T();
		}

		// Scale the alpha by the number of lerps
		const int32 NumberOfLerps = (Values.Num() - 1);
		const float ScaledAlpha = (InAlpha * NumberOfLerps);

		// Get the indexes of A and B to lerp between
		const int32 IndexOfA = FMath::FloorToInt(ScaledAlpha);
		const int32 IndexOfB = FMath::CeilToInt(ScaledAlpha);

		// Get the alpha relative to this A and B (the decimal part of our ScaledAlpha)
		const float LerpAlpha = ScaledAlpha - IndexOfA;

		// Perform the lerp
		// Also, in case the caller gave us an alpha outside of 0 and 1, ensure that we don't access out of range indexes
		const int32 SafeIndexOfA = FMath::Clamp(IndexOfA, 0, Values.Num() - 1);
		const int32 SafeIndexOfB = FMath::Clamp(IndexOfB, 0, Values.Num() - 1);
		return FMath::Lerp<T>(Values[SafeIndexOfA], Values[SafeIndexOfB], LerpAlpha);
	}
};
