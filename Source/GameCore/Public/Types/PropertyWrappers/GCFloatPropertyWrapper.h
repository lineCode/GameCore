// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GCPropertyWrapper.h"

#include "GCFloatPropertyWrapper.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGCFloatValueChange, const float&, OldValue, const float&, NewValue);


/**
 * FGCFloatPropertyWrapper
 * 
 * A float property that provides 2 features:
 *		- Automatic MARK_PROPERTY_DIRTY() on value change
 *			- This requires Push Model enabled
 *		- Automatic broadcasting of a delegate on value change
 */
USTRUCT(BlueprintType)
struct GAMECORE_API FGCFloatPropertyWrapper
{
	GENERATED_BODY()

	GC_PROPERTY_WRAPPER_MEMBERS(float, Float, 0.f);

private:
	/** The pointer to the FProperty on our outer's UClass */
	UPROPERTY(Transient)
		TFieldPath<FProperty> Property;

	/** The pointer to our outer - needed for replication */
	UPROPERTY(Transient)
		TWeakObjectPtr<UObject> PropertyOwner;

	/** The actual value of this float property */
	UPROPERTY(EditAnywhere)
		float Value;
};

template<>
struct TStructOpsTypeTraits<FGCFloatPropertyWrapper> : public TStructOpsTypeTraitsBase2<FGCFloatPropertyWrapper>
{
	enum
	{
		CG_PROPERTY_WRAPPER_STRUCT_OPS_TYPE_TRAITS_ENUMERATORS
	};
};
