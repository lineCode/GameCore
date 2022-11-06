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
 * 
 * The reason we need sub-classes to declare the Value member for us is because we want it to be a UPROPERTY and generated code (e.g., custom macros) will not be seen by Unreal Header Tool.
 */
USTRUCT(BlueprintType)
struct GAMECORE_API FGCPropertyWrapperBase
{
	GENERATED_BODY()

public:
	FGCPropertyWrapperBase();
	virtual ~FGCPropertyWrapperBase() { }
protected:
	FGCPropertyWrapperBase(UObject* InPropertyOwner, const FName& InPropertyName, const UScriptStruct* InChildScriptStruct); // initialization is intended only for child structs
public:

	/** If true, will MARK_PROPERTY_DIRTY() when Value is set */
	uint8 bMarkNetDirtyOnChange : 1;

	/** Marks the property dirty */
	void MarkNetDirty();

	FProperty* GetProperty() const { return Property.Get(); }
	UObject* GetPropertyOwner() const { return PropertyOwner.Get(); }
	FName GetPropertyName() const { return Property->GetFName(); }

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
{ }\
TPropertyWrapperType(UObject* InPropertyOwner, const FName& InPropertyName, const TValueType& InValue = DefaultValue)\
	: FGCPropertyWrapperBase(InPropertyOwner, InPropertyName, GetScriptStruct())\
	, Value(InValue)\
{ }\
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
/** Our custom serialization for this struct */\
virtual bool Serialize(FArchive& InOutArchive)\
{\
	/* It makes more sense to do serialization work in here since the engine uses this more than operator<<().*/\
	InOutArchive << Value;\
	return true;\
}\
\
/** Uses our custom serialization */\
friend FArchive& operator<<(FArchive& InOutArchive, TPropertyWrapperType& InOut##ValueTypeName##PropertyWrapper)\
{\
	InOut##ValueTypeName##PropertyWrapper.Serialize(InOutArchive);\
	return InOutArchive;\
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
