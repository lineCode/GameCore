// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GCFloatPropertyWrapper.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFloatValueChange, const float&, OldValue, const float&, NewValue);


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

public:
	FGCFloatPropertyWrapper();
	FGCFloatPropertyWrapper(UObject* InPropertyOwner, const FName& PropertyName, const float& InValue = 0.f);
	virtual ~FGCFloatPropertyWrapper() { }

	/** An easy conversion from this struct to float. This allows FGCFloatPropertyWrapper to be treated as a float in code */
	operator float() const
	{
		return Value;
	}

	/** Broadcasts ValueChangeDelegate and does MARK_PROPERTY_DIRTY() */
	float operator=(const float& NewValue);


	virtual UScriptStruct* GetScriptStruct() const { return StaticStruct(); }

	/** Our custom replication for this struct (we only want to replicate Value) */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);


	/** Broadcasted whenever Value changes */
	FFloatValueChange ValueChangeDelegate;

	/** If true, will MARK_PROPERTY_DIRTY() when Value is set */
	uint8 bMarkNetDirtyOnChange : 1;

	/** Marks the property dirty */
	void MarkNetDirty();


	/** Debug string displaying the property name and value */
	FString GetDebugString(bool bDetailedDebugString = false) const;

	FProperty* GetProperty() const { return Property.Get(); }
	UObject* GetPropertyOwner() const { return PropertyOwner.Get(); }
	FName GetPropertyName() const { return Property->GetFName(); }

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
		WithNetSerializer = true
	};
};
