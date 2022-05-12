// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_DrawDebugHelpers.h"

#include "DrawDebugHelpers.h"



void UBFL_DrawDebugHelpers::DrawDebugCollisionShape(const UWorld* InWorld, const FVector& Center, const FCollisionShape& CollisionShape, const FQuat& Rotation, const FColor& Color, const int32 Segments, const bool bPersistentLines, const float LifeTime, const uint8 DepthPriority, const float Thickness)
{
#if ENABLE_DRAW_DEBUG
	switch (CollisionShape.ShapeType)
	{
		case ECollisionShape::Box:
		{
			DrawDebugBox(InWorld, Center, CollisionShape.GetExtent(), Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			break;
		}
		case  ECollisionShape::Sphere:
		{
			DrawDebugSphere(InWorld, Center, CollisionShape.GetSphereRadius(), Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			break;
		}
		case ECollisionShape::Capsule:
		{
			// NOTE: Segments is not used here because DrawDebugCapsule() hard codes it
			DrawDebugCapsule(InWorld, Center, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			break;
		}
		case ECollisionShape::Line:
		{
			DrawDebugPoint(InWorld, Center, Thickness * 10, Color, bPersistentLines, LifeTime, DepthPriority);
			break;
		}
	}
#endif // ENABLE_DRAW_DEBUG
}
