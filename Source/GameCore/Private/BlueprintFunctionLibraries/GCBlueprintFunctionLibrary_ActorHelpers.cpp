// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/GCBlueprintFunctionLibrary_ActorHelpers.h"



// NOTE: This code is nearly duplicate from UGCBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuter() - if you change one, change the other.
AActor* UGCBlueprintFunctionLibrary_ActorHelpers::GetTypedOwner(const AActor* InSelfActor, const TSubclassOf<AActor> InOwnerClass)
{
	if (IsValid(InSelfActor))
	{
		// Works similar to UObjectBaseUtility::GetTypedOuter()
		for (AActor* NextOwner = InSelfActor->GetOwner(); IsValid(NextOwner); NextOwner = NextOwner->GetOwner())
		{
			if (NextOwner->IsA(InOwnerClass))
			{
				return NextOwner;
			}
		}
	}

	return nullptr;
}
AActor* UGCBlueprintFunctionLibrary_ActorHelpers::GetTypedOwnerIncludingSelf(AActor* InSelfActor, const TSubclassOf<AActor> InOwnerClass)
{
	if (IsValid(InSelfActor) && InSelfActor->IsA(InOwnerClass))
	{
		return InSelfActor;
	}

	return GetTypedOwner(InSelfActor, InOwnerClass);
}
AActor* UGCBlueprintFunctionLibrary_ActorHelpers::GetTypedOwnerOfComponent(const UActorComponent* InSelfComponent, const TSubclassOf<AActor> InOwnerClass)
{
	if (IsValid(InSelfComponent))
	{
		return GetTypedOwnerIncludingSelf(InSelfComponent->GetOwner(), InOwnerClass);
	}

	return nullptr;
}
