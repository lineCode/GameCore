// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GCPropertyWrapperBase.h"

#include "GCFloatPropertyWrapper.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGCFloatValueChange, const float&, OldValue, const float&, NewValue); // make the required delegate type for this property wrapper


USTRUCT(BlueprintType)
struct GAMECORE_API FGCFloatPropertyWrapper : public FGCPropertyWrapperBase
{
	GENERATED_BODY()

	GC_PROPERTY_WRAPPER_MEMBERS(float, Float, 0.f);

private:
	/** The actual value of this float property */
	UPROPERTY(EditAnywhere)
		float Value;
};

template<>
struct TStructOpsTypeTraits<FGCFloatPropertyWrapper> : public TStructOpsTypeTraitsBase2<FGCFloatPropertyWrapper>
{
	enum
	{
		WithNetSerializer = true // required for the property wrapper
	};
};
