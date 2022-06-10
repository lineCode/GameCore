// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/HLBlueprintFunctionLibrary_InterfaceHelpers.h"



// NOTE: This code is nearly duplicate from UHLBlueprintFunctionLibrary_ActorHelpers::GetTypedOwner() - if you change one, change the other
UObject* UHLBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuter(const UObject* InObject, const TSubclassOf<UInterface> InOuterClass)
{
	if (InObject == nullptr)
	{
		return nullptr;
	}

	UObject* Result = nullptr;

	// Works similar to UObjectBaseUtility::GetTypedOuter()
	for (UObject* NextOuter = InObject->GetOuter(); (Result == nullptr && NextOuter != nullptr); NextOuter = NextOuter->GetOuter())
	{
		if (NextOuter->GetClass()->ImplementsInterface(InOuterClass))
		{
			Result = NextOuter;
		}
	}

	return Result;
}
