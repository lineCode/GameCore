// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GCBlueprintFunctionLibrary_InterfaceHelpers.generated.h"



/**
 * A collection of common Interface helpers
 */
UCLASS()
class GAMECORE_API UGCBlueprintFunctionLibrary_InterfaceHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Traverses the outer chain searching for the next object of a certain interface type.
	 * An alternative to UObjectBaseUtility::GetTypedOuter() that works for interfaces.
	 * Takes in UClass for the outer type.
	 */
	UFUNCTION(BlueprintPure, Category = "InterfaceHelpers")
		static UObject* GetInterfaceTypedOuter(const UObject* InSelfObject, const TSubclassOf<UInterface> InOuterClass);
	/**
	 * Version of GetInterfaceTypedOuter() that includes the self object for consideration.
	 */
	UFUNCTION(BlueprintPure, Category = "InterfaceHelpers")
		static UObject* GetInterfaceTypedOuterIncludingSelf(UObject* InSelfObject, const TSubclassOf<UInterface> InOuterClass);

	/**
	 * Version of GetInterfaceTypedOuter() that returns the result in its type.
	 */
	template <class IInterfaceType, class UInterfaceType>
	static IInterfaceType* GetInterfaceTypedOuterCasted(const UObject* InSelfObject);
	/**
	 * Version of GetInterfaceTypedOuterIncludingSelf() that returns the result in its type.
	 */
	template <class IInterfaceType, class UInterfaceType>
	static IInterfaceType* GetInterfaceTypedOuterIncludingSelfCasted(UObject* InSelfObject);

};


template <class IInterfaceType, class UInterfaceType>
IInterfaceType* UGCBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuterCasted(const UObject* InSelfObject)
{
	return Cast<IInterfaceType>(GetInterfaceTypedOuter(InSelfObject, UInterfaceType::StaticClass()));
}
template <class IInterfaceType, class UInterfaceType>
IInterfaceType* UGCBlueprintFunctionLibrary_InterfaceHelpers::GetInterfaceTypedOuterIncludingSelfCasted(UObject* InSelfObject)
{
	return Cast<IInterfaceType>(GetInterfaceTypedOuterIncludingSelf(InSelfObject, UInterfaceType::StaticClass()));
}
