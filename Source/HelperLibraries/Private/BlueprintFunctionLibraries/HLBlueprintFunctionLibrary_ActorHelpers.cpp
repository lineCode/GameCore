// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/HLBlueprintFunctionLibrary_ActorHelpers.h"



// NOTE: This code is nearly duplicate from UHLBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuter() - if you change one, change the other.
AActor* UHLBlueprintFunctionLibrary_ActorHelpers::GetTypedOwner(const AActor* InActor, const TSubclassOf<AActor> InOwnerClass)
{
	if (InActor == nullptr)
	{
		return nullptr;
	}

	AActor* Result = nullptr;

	// Works similar to UObjectBaseUtility::GetTypedOuter()
	for (AActor* NextOwner = InActor->GetOwner(); (Result == nullptr && NextOwner != nullptr); NextOwner = NextOwner->GetOwner())
	{
		if (NextOwner->IsA(InOwnerClass))
		{
			Result = NextOwner;
		}
	}

	return Result;
}
