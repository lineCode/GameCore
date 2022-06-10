// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/HLBlueprintFunctionLibrary_InterfaceHelpers.h"



// NOTE: This code is nearly duplicate from UHLBlueprintFunctionLibrary_ActorHelpers::GetTypedOwner() - if you change one, change the other
UObject* UHLBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuter(const UObject* InSelfObject, const TSubclassOf<UInterface> InOuterClass)
{
	if (IsValid(InSelfObject))
	{
		// Works similar to UObjectBaseUtility::GetTypedOuter()
		for (UObject* NextOuter = InSelfObject->GetOuter(); IsValid(NextOuter); NextOuter = NextOuter->GetOuter())
		{
			if (NextOuter->GetClass()->ImplementsInterface(InOuterClass))
			{
				return NextOuter;
			}
		}
	}

	return nullptr;
}
UObject* UHLBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuterIncludingSelf(UObject* InSelfObject, const TSubclassOf<UInterface> InOuterClass)
{
	if (IsValid(InSelfObject) && InSelfObject->GetClass()->ImplementsInterface(InOuterClass))
	{
		return InSelfObject;
	}

	return GetInterfaceTypedOuter(InSelfObject, InOuterClass);
}
