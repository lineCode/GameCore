// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "HLBlueprintFunctionLibrary_ActorHelpers.generated.h"



/**
 * A collection of common AActor helpers
 */
UCLASS()
class HELPERLIBRARIES_API UHLBlueprintFunctionLibrary_ActorHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Traverses the owner chain searching for the next Actor of a certain actor type.
	 * An alternative to UObjectBaseUtility::GetTypedOuter() that uses AActor::Owner.
	 * Takes in UClass for the owner type.
	 */
	UFUNCTION(BlueprintPure, Category = "ActorHelpers")
		static AActor* GetTypedOwner(const AActor* InActor, const TSubclassOf<AActor> InOwnerClass);

	/**
	 * Version of GetTypedOwner() that returns the result in its type.
	 */
	template <class T>
	static T* GetTypedOwnerCasted(const AActor* InActor);

};


template <class T>
T* UHLBlueprintFunctionLibrary_ActorHelpers::GetTypedOwnerCasted(const AActor* InActor)
{
	return Cast<T>(GetTypedOwner(InActor, T::StaticClass()));
}
