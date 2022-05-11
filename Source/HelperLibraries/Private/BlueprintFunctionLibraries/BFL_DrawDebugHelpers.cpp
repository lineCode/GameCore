// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/BFL_DrawDebugHelpers.h"

#include "DrawDebugHelpers.h"



void UBFL_DrawDebugHelpers::DrawDebugCollisionShape(const FCollisionShape& CollisionShape, const UWorld* InWorld, const FVector& Center, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, const FQuat& Rotation)
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
			UE_LOG(LogDrawDebugHelpers, Warning, TEXT("%s() Caller passed in a rotation when drawing a debug sphere, but there is no need to since debug spheres don't have rotation"), ANSI_TO_TCHAR(__FUNCTION__));
			DrawDebugSphere(InWorld, Center, CollisionShape.GetSphereRadius(), Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			break;
		}
		case ECollisionShape::Capsule:
		{
			DrawDebugCapsule(InWorld, Center, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), Rotation, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			break;
		}
		case ECollisionShape::Line:
		{
			UE_LOG(LogDrawDebugHelpers, Error, TEXT("%s() not implemented exception"), ANSI_TO_TCHAR(__FUNCTION__));
			check(0);
			break;
		}
	}
#endif // ENABLE_DRAW_DEBUG
}
