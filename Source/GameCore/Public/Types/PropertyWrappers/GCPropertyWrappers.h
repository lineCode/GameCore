// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GCPropertyWrapperBase.h"

#include "GCPropertyWrappers.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGCFloatValueChange, const float&, OldValue, const float&, NewValue); // make the required delegate type for this property wrapper
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGCInt32ValueChange, const int32&, OldValue, const int32&, NewValue);


USTRUCT()
struct GAMECORE_API FGCFloatPropertyWrapper : public FGCPropertyWrapperBase
{
	GENERATED_BODY()

	GC_PROPERTY_WRAPPER_MEMBERS(float, Float, 0.f);

	/** Our custom serialization for this struct */
	virtual bool Serialize(FArchive& InOutArchive)
	{
		// It makes more sense to do serialization work in here since the engine uses this more than operator<<().
		InOutArchive << Value;
		return true;
	}

	/** Uses our custom serialization */
	friend FArchive& operator<<(FArchive& InOutArchive, FGCFloatPropertyWrapper& InOutFloatPropertyWrapper)
	{
		InOutFloatPropertyWrapper.Serialize(InOutArchive);
		return InOutArchive;
	}

	/** Our custom replication for this struct (we only want to replicate Value) */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Value;
		return true;
	}

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
		WithSerializer = true, // required for the property wrapper
		WithNetSerializer = true // required for the property wrapper
	};
};


USTRUCT()
struct GAMECORE_API FGCInt32PropertyWrapper : public FGCPropertyWrapperBase
{
	GENERATED_BODY()

	GC_PROPERTY_WRAPPER_MEMBERS(int32, Int32, 0);

	virtual bool Serialize(FArchive& InOutArchive)
	{
		InOutArchive << Value;
		return true;
	}

	friend FArchive& operator<<(FArchive& InOutArchive, FGCInt32PropertyWrapper& InOutFloatPropertyWrapper)
	{
		InOutFloatPropertyWrapper.Serialize(InOutArchive);
		return InOutArchive;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Value;
		return true;
	}

private:
	UPROPERTY(EditAnywhere)
		int32 Value;
};

template<>
struct TStructOpsTypeTraits<FGCInt32PropertyWrapper> : public TStructOpsTypeTraitsBase2<FGCInt32PropertyWrapper>
{
	enum
	{
		WithSerializer = true,
		WithNetSerializer = true
	};
};
