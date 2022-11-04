// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameCore/Private/Utilities/GCLogCategories.h"



#define GC_VALUE_CHANGE_DELEGATE_TYPE(ValueTypeName) PREPROCESSOR_JOIN(FGC, PREPROCESSOR_JOIN(ValueTypeName, ValueChange))
#define GC_PROPERTY_WRAPPER_TYPE(ValueTypeName) PREPROCESSOR_JOIN(FGC, PREPROCESSOR_JOIN(ValueTypeName, PropertyWrapper))


//  BEGIN Property wrapper member variables
#define GC_PROPERTY_WRAPPER_MEMBER_VARIABLE_VALUE_CHANGE_DELEGATE(ValueType, ValueTypeName) \
/** Broadcasted whenever Value changes */\
GC_VALUE_CHANGE_DELEGATE_TYPE(ValueTypeName) ValueChangeDelegate;

#define GC_PROPERTY_WRAPPER_MEMBER_VARIABLE_B_MARK_NET_DIRTY_ON_CHANGE \
/** If true, will MARK_PROPERTY_DIRTY() when Value is set */\
uint8 bMarkNetDirtyOnChange : 1;
//  END Property wrapper member variables

//  BEGIN Property wrapper member functions
#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_CONSTRUCTOR_DEFAULT(ValueType, ValueTypeName, DefaultValue) \
GC_PROPERTY_WRAPPER_TYPE(ValueTypeName)()\
	: bMarkNetDirtyOnChange(false)\
	, Property(nullptr)\
	, PropertyOwner(nullptr)\
	, Value(DefaultValue)\
{\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_CONSTRUCTOR(ValueType, ValueTypeName, DefaultValue) \
GC_PROPERTY_WRAPPER_TYPE(ValueTypeName)(UObject* InPropertyOwner, const FName& PropertyName, const ValueType& InValue = DefaultValue)\
	: FGCFloatPropertyWrapper()\
{\
	PropertyOwner = InPropertyOwner;\
	Value = InValue;\
	\
	\
	/* Get the FProperty so we can use push model with it */\
	Property = FindFProperty<FProperty>(PropertyOwner->GetClass(), PropertyName);\
	\
	/* Ensure this property exists on the owner and that this property is an FGCFloatPropertyWrapper! */\
	if constexpr (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))\
	{\
		if (!Property.Get())\
		{\
			UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): The given PropertyName \"%s\" was not found on the PropertyOwner ``%s``. Ensure correct spelling for the property you are looking for and make sure that it is a UPROPERTY so we can find it!"), ANSI_TO_TCHAR(__FUNCTION__), *(PropertyName.ToString()), *(InPropertyOwner->GetName()));\
			check(0);\
		}\
		else if (!(CastField<FStructProperty>(Property.Get()) && CastField<FStructProperty>(Property.Get())->Struct == FGCFloatPropertyWrapper::StaticStruct()))\
		{\
			UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): The given FProperty ``%s::%s`` is not a(n) %s!"), ANSI_TO_TCHAR(__FUNCTION__), *(InPropertyOwner->GetClass()->GetName()), *(Property->GetFName().ToString()), *(FGCFloatPropertyWrapper::StaticStruct()->GetName()));\
			check(0);\
		}\
	}\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_DESTRUCTOR(ValueTypeName) \
virtual ~GC_PROPERTY_WRAPPER_TYPE(ValueTypeName)() { }

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_OPERATOR_IMPLICIT_CONVERSION(ValueType) \
/** An easy conversion from this struct to ValueType. This allows FGCFloatPropertyWrapper to be treated as a ValueType in code */\
operator ValueType() const\
{\
	return Value;\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_OPERATOR_EQUALS(ValueType, ValueTypeName) \
/** Broadcasts ValueChangeDelegate and does MARK_PROPERTY_DIRTY() */\
ValueType operator=(const ValueType& NewValue)\
{\
	const ValueType OldValue = Value;\
	Value = NewValue;\
	\
	if (NewValue != OldValue)\
	{\
		ValueChangeDelegate.Broadcast(OldValue, NewValue);\
		\
		if (bMarkNetDirtyOnChange)\
		{\
			MarkNetDirty();\
		}\
	}\
	\
	return Value;\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_SCRIPT_STRUCT() \
virtual UScriptStruct* GetScriptStruct() const { return StaticStruct(); }

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_NET_SERIALIZE() \
/** Our custom replication for this struct (we only want to replicate Value) */\
virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)\
{\
	/* We can save on network when the Value is zero by skipping the 4-byte float serialization and instead serialize a single bit indicating whether the Value is zero */\
	bool bValueEqualsZero = false;\
	\
	\
	uint8 RepBits;\
	if (Ar.IsSaving())\
	{\
		/* We are a writer, so lets find some optimizations and pack them into RepBits */\
		\
		if (Value == 0.f)\
		{\
			bValueEqualsZero = true;\
		}\
		\
		\
		RepBits = (bValueEqualsZero << 0);\
	}\
	\
	/* Pack/unpack our RepBits into/outof the Archive */\
	Ar.SerializeBits(&RepBits, 1);\
	\
	if (Ar.IsLoading())\
	{\
		/* We are a reader, so lets unpack our optimization bools out of RepBits */\
		\
		bValueEqualsZero = (RepBits & (1 << 0));\
	}\
	\
	\
	if (Ar.IsSaving())\
	{\
		if (!bValueEqualsZero) /* no need to serialize Value if it is zero because we have a bit indicating that already */\
		{\
			Ar << Value;\
		}\
	}\
	\
	if (Ar.IsLoading())\
	{\
		float NewValue = 0;\
		if (!bValueEqualsZero) /* do not deserialize Value if it is zero */\
		{\
			Ar << NewValue;\
		}\
		\
		/* Assign Value using our operator (so that ValueChangeDelegate may get broadcasted) */\
		operator=(NewValue);\
	}\
	\
	\
	\
	bOutSuccess = true;\
	return true;\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_MARK_NET_DIRTY() \
/** Marks the property dirty */\
void MarkNetDirty()\
{\
	if constexpr (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))\
	{\
		/* Ensure this property is replicated */\
		if (!Property->HasAnyPropertyFlags(EPropertyFlags::CPF_Net)) /* i want to use IS_PROPERTY_REPLICATED() here but, in non-Editor target, we for some reason get an "error C3861: 'IS_PROPERTY_REPLICATED': identifier not found" */\
		{\
			UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): Tried to mark property net dirty to replicate, but ``%s::%s`` is not replicated! Cannot replicate!"), ANSI_TO_TCHAR(__FUNCTION__), GetData(PropertyOwner->GetClass()->GetName()), GetData(Property->GetName()));\
		}\
		/* Ensure Push Model is enabled */\
		if (!IS_PUSH_MODEL_ENABLED())\
		{\
			UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): Tried to mark property ``%s::%s`` net dirty, but Push Model is disabled! Cannot replicate!"), ANSI_TO_TCHAR(__FUNCTION__), GetData(PropertyOwner->GetClass()->GetName()), GetData(Property->GetName()));\
		}\
	}\
	\
	\
	MARK_PROPERTY_DIRTY(PropertyOwner, Property);\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_DEBUG_STRING() \
/** Debug string displaying the property name and value */\
FString GetDebugString(bool bDetailedDebugString = false) const\
{\
	if (bDetailedDebugString)\
	{\
		return PropertyOwner->GetPathName(PropertyOwner->GetTypedOuter<ULevel>()) + TEXT(".") + Property->GetName() + TEXT(": ") + FString::SanitizeFloat(Value);\
	}\
	\
	return Property->GetName() + TEXT(": ") + FString::SanitizeFloat(Value);\
}

#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_PROPERTY() \
FProperty* GetProperty() const { return Property.Get(); }
#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_PROPERTY_OWNER() \
UObject* GetPropertyOwner() const { return PropertyOwner.Get(); }
#define GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_PROPERTY_NAME() \
FName GetPropertyName() const { return Property->GetFName(); }
//  END Property wrapper member functions


/** Include this in the public section of your struct body. */
#define GC_PROPERTY_WRAPPER_MEMBERS_PUBLIC(ValueType, ValueTypeName, DefaultValue) \
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_CONSTRUCTOR_DEFAULT(ValueType, ValueTypeName, DefaultValue)\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_CONSTRUCTOR(ValueType, ValueTypeName, DefaultValue)\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_DESTRUCTOR(ValueTypeName)\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_OPERATOR_IMPLICIT_CONVERSION(ValueType)\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_OPERATOR_EQUALS(ValueType, ValueTypeName)\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_SCRIPT_STRUCT()\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_NET_SERIALIZE()\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_MARK_NET_DIRTY()\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_DEBUG_STRING()\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_PROPERTY()\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_PROPERTY_OWNER()\
GC_PROPERTY_WRAPPER_MEMBER_FUNCTION_GET_PROPERTY_NAME()\
GC_PROPERTY_WRAPPER_MEMBER_VARIABLE_VALUE_CHANGE_DELEGATE(ValueType, ValueTypeName)\
GC_PROPERTY_WRAPPER_MEMBER_VARIABLE_B_MARK_NET_DIRTY_ON_CHANGE

/** Include this in your TStructOpsTypeTraits<GC_PROPERTY_WRAPPER_TYPE()>::enum */
#define CG_PROPERTY_WRAPPER_STRUCT_OPS_TYPE_TRAITS_ENUMERATORS \
WithNetSerializer = true
