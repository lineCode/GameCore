// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GCPropertyWrapperBase.h"

#include "GCPropertyWrappers.generated.h"



USTRUCT(BlueprintType)
struct GAMECORE_API FGCFloatPropertyWrapper : public FGCPropertyWrapperBase
{
	GENERATED_BODY()

	GC_PROPERTY_WRAPPER_BODY(float, Float, 0.f);

	virtual bool Serialize(FArchive& InOutArchive) override
	{
		InOutArchive << Value;
		return true;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar << Value;
		return true;
	}

	virtual FString ToString() const override { return FString::SanitizeFloat(Value); }

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


USTRUCT(BlueprintType)
struct GAMECORE_API FGCInt32PropertyWrapper : public FGCPropertyWrapperBase
{
	GENERATED_BODY()

	GC_PROPERTY_WRAPPER_BODY(int32, Int32, 0);

	virtual bool Serialize(FArchive& InOutArchive) override
	{
		InOutArchive << Value;
		return true;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar << Value;
		return true;
	}

	virtual FString ToString() const override { return FString::FromInt(Value); }

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
