// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "BFL_InterfaceHelpers.generated.h"



/**
 * A collection of common Interface helpers
 */
UCLASS()
class HELPERLIBRARIES_API UBFL_InterfaceHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	 * Traverses the outer chain searching for the next object of a certain Interface type
	 */
	template <typename InterfaceType, typename UInterfaceType, typename T>
	static InterfaceType* GetInterfaceTypedOuter(const T* Object)
	{
		return GetInterfaceTypedOuter<InterfaceType>(Object, UInterfaceType::StaticClass());
	}

	/**
	 * Traverses the outer chain searching for the next object of a certain Interface type
	 */
	template <typename InterfaceType, typename T>
	static InterfaceType* GetInterfaceTypedOuter(const T* Object, const UClass* InterfaceClass)
	{
		InterfaceType* RetVal = nullptr;

		for (UObject* NextOuter = Object->GetOuter(); (RetVal == nullptr && NextOuter != nullptr); NextOuter = NextOuter->GetOuter())
		{
			if (NextOuter->GetClass()->ImplementsInterface(InterfaceClass))
			{
				RetVal = Cast<InterfaceType>(NextOuter);
			}
		}

		return RetVal;
	}

};
