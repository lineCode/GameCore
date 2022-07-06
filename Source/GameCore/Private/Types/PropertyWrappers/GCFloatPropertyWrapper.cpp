// Fill out your copyright notice in the Description page of Project Settings.


#include "Types\PropertyWrappers\GCFloatPropertyWrapper.h"

#include "Net/Core/PushModel/PushModel.h"



FGCFloatPropertyWrapper::FGCFloatPropertyWrapper()
	: bMarkNetDirtyOnChange(false)
	, Property(nullptr)
	, PropertyOwner(nullptr)
	, Value(0.f)
{
}
FGCFloatPropertyWrapper::FGCFloatPropertyWrapper(UObject* InPropertyOwner, const FName& PropertyName, const float& InValue)
	: FGCFloatPropertyWrapper()
{
	PropertyOwner = InPropertyOwner;
	Value = InValue;


	// Get the FProperty so we can use push model with it
	Property = FindFProperty<FProperty>(PropertyOwner->GetClass(), PropertyName);

	// Ensure this property exists on the owner and that this property is an FGCFloatPropertyWrapper!
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!Property.Get())
	{
		UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): The given PropertyName \"%s\" was not found on the PropertyOwner ``%s``. Ensure correct spelling for the property you are looking for and make sure that it is a UPROPERTY so we can find it!"), ANSI_TO_TCHAR(__FUNCTION__), *(PropertyName.ToString()), *(InPropertyOwner->GetName()));
		check(0);
	}
	else if (!(CastField<FStructProperty>(Property.Get()) && CastField<FStructProperty>(Property.Get())->Struct == FGCFloatPropertyWrapper::StaticStruct()))
	{
		UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): The given FProperty ``%s::%s`` is not a(n) %s!"), ANSI_TO_TCHAR(__FUNCTION__), *(InPropertyOwner->GetClass()->GetName()), *(Property->GetFName().ToString()), *(FGCFloatPropertyWrapper::StaticStruct()->GetName()));
		check(0);
	}
#endif
}


float FGCFloatPropertyWrapper::operator=(const float& NewValue)
{
	const float OldValue = Value;
	Value = NewValue;

	if (NewValue != OldValue)
	{
		ValueChangeDelegate.Broadcast(OldValue, NewValue);

		if (bMarkNetDirtyOnChange)
		{
			MarkNetDirty();
		}
	}

	return Value;
}

void FGCFloatPropertyWrapper::MarkNetDirty()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Ensure this property is replicated
	if (!IS_PROPERTY_REPLICATED(Property))
	{
		UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): Tried to mark property net dirty to replicate, but ``%s::%s`` is not replicated! Cannot replicate!"), ANSI_TO_TCHAR(__FUNCTION__), GetData(PropertyOwner->GetClass()->GetName()), GetData(Property->GetName()));
	}
	// Ensure Push Model is enabled
	if (!IS_PUSH_MODEL_ENABLED())
	{
		UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): Tried to mark property ``%s::%s`` net dirty, but Push Model is disabled! Cannot replicate!"), ANSI_TO_TCHAR(__FUNCTION__), GetData(PropertyOwner->GetClass()->GetName()), GetData(Property->GetName()));
	}
#endif

	MARK_PROPERTY_DIRTY(PropertyOwner, Property);
}

bool FGCFloatPropertyWrapper::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// We can save on network when the Value is zero by skipping the 4-byte float serialization and instead serialize a single bit indicating whether the Value is zero
	bool bValueEqualsZero = false;


	uint8 RepBits;
	if (Ar.IsSaving())
	{
		// We are a writer, so lets find some optimizations and pack them into RepBits

		if (Value == 0.f)
		{
			bValueEqualsZero = true;
		}


		RepBits = (bValueEqualsZero << 0);
	}

	// Pack/unpack our RepBits into/outof the Archive
	Ar.SerializeBits(&RepBits, 1);

	if (Ar.IsLoading())
	{
		// We are a reader, so lets unpack our optimization bools out of RepBits

		bValueEqualsZero = (RepBits & (1 << 0));
	}


	if (Ar.IsSaving())
	{
		if (!bValueEqualsZero) // no need to serialize Value if it is zero because we have a bit indicating that already
		{
			Ar << Value;
		}
	}

	if (Ar.IsLoading())
	{
		float NewValue = 0;
		if (!bValueEqualsZero) // do not deserialize Value if it is zero
		{
			Ar << NewValue;
		}

		// Assign Value using our operator (so that ValueChangeDelegate may get broadcasted)
		operator=(NewValue);
	}



	bOutSuccess = true;
	return true;
}


FString FGCFloatPropertyWrapper::GetDebugString(bool bDetailedDebugString) const
{
	if (bDetailedDebugString)
	{
		return PropertyOwner->GetPathName(PropertyOwner->GetTypedOuter<ULevel>()) + TEXT(".") + Property->GetName() + TEXT(": ") + FString::SanitizeFloat(Value);
	}

	return Property->GetName() + TEXT(": ") + FString::SanitizeFloat(Value);
}
