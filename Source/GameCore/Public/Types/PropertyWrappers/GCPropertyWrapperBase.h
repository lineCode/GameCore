// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/Core/PushModel/PushModel.h"
#include "GameCore/Private/Utilities/GCLogCategories.h"

#include "GCPropertyWrapperBase.generated.h"



/**
 * FGCPropertyWrapperBase
 * 
 * Property wrappers provide 2 main features
 *  - Automatic dirtying for push model
 *  - Automatic delegate broadcasting
 * 
 * This base struct holds the non-value-type-related members.
 * Subclasses are able control the type by declaring the actual Value property.
 * See GC_PROPERTY_WRAPPER_MEMBERS for boilerplate code and subclass requirements.
 */
USTRUCT(BlueprintType)
struct GAMECORE_API FGCPropertyWrapperBase
{
	GENERATED_BODY()

public:
	FGCPropertyWrapperBase();
	virtual ~FGCPropertyWrapperBase() { }

	/** If true, will MARK_PROPERTY_DIRTY() when Value is set */
	uint8 bMarkNetDirtyOnChange : 1;

protected:
	/** The pointer to the FProperty on our outer's UClass */
	UPROPERTY(Transient)
		TFieldPath<FProperty> Property;

	/** The pointer to our outer - needed for replication */
	UPROPERTY(Transient)
		TWeakObjectPtr<UObject> PropertyOwner;
};


/**
 * Boilerplate code for subclasses of FGCPropertyWrapperBase. Include this anywhere in your struct body.
 * Uses your declared Value member.
 * Uses your defined value change delegate type.
 * Assumes you have the WithNetSerializer type trait.
 */
#define GC_PROPERTY_WRAPPER_MEMBERS(ValueType, ValueTypeName, DefaultValue) \
private:\
typedef FGC##ValueTypeName##PropertyWrapper TPropertyWrapperType;\
typedef ValueType TValueType;\
typedef FGC##ValueTypeName##ValueChange TValueChangeDelegateType;\
\
public:\
\
/** Broadcasted whenever Value changes */\
TValueChangeDelegateType ValueChangeDelegate;\
\
TPropertyWrapperType()\
	: Value(DefaultValue)\
{\
}\
\
TPropertyWrapperType(UObject* InPropertyOwner, const FName& PropertyName, const TValueType& InValue = DefaultValue)\
	: TPropertyWrapperType()\
{\
	PropertyOwner = InPropertyOwner;\
	Value = InValue;\
	\
	\
	/* Get the FProperty so we can use push model with it */\
	Property = FindFProperty<FProperty>(PropertyOwner->GetClass(), PropertyName);\
	\
	/* Ensure this property exists on the owner and that this property is a TPropertyWrapperType! */\
	if constexpr (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))\
	{\
		if (!Property.Get())\
		{\
			UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): The given PropertyName \"%s\" was not found on the PropertyOwner ``%s``. Ensure correct spelling for the property you are looking for and make sure that it is a UPROPERTY so we can find it!"), ANSI_TO_TCHAR(__FUNCTION__), *(PropertyName.ToString()), *(InPropertyOwner->GetName()));\
			check(0);\
		}\
		else if (!(CastField<FStructProperty>(Property.Get()) && CastField<FStructProperty>(Property.Get())->Struct == TPropertyWrapperType::StaticStruct()))\
		{\
			UE_LOG(LogGCPropertyWrapper, Error, TEXT("%s(): The given FProperty ``%s::%s`` is not a(n) %s!"), ANSI_TO_TCHAR(__FUNCTION__), *(InPropertyOwner->GetClass()->GetName()), *(Property->GetFName().ToString()), *(TPropertyWrapperType::StaticStruct()->GetName()));\
			check(0);\
		}\
	}\
}\
\
~FGC##ValueTypeName##PropertyWrapper() { }\
\
/** An easy conversion from this struct to TValueType. This allows TPropertyWrapperType to be treated as a TValueType in code */\
operator TValueType() const\
{\
	return Value;\
}\
\
/** Broadcasts ValueChangeDelegate and does MARK_PROPERTY_DIRTY() */\
TValueType operator=(const TValueType& NewValue)\
{\
	const TValueType OldValue = Value;\
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
}\
\
virtual UScriptStruct* GetScriptStruct() const { return StaticStruct(); }\
\
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
}\
\
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
}\
\
/** Debug string displaying the property name and value */\
FString GetDebugString(bool bDetailedDebugString = false) const\
{\
	if (bDetailedDebugString)\
	{\
		return PropertyOwner->GetPathName(PropertyOwner->GetTypedOuter<ULevel>()) + TEXT(".") + Property->GetName() + TEXT(": ") + FString::SanitizeFloat(Value);\
	}\
	\
	return Property->GetName() + TEXT(": ") + FString::SanitizeFloat(Value);\
}\
\
FProperty* GetProperty() const { return Property.Get(); }\
UObject* GetPropertyOwner() const { return PropertyOwner.Get(); }\
FName GetPropertyName() const { return Property->GetFName(); }
