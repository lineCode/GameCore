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
 * 
 * Subclasses implement Serialize(), NetSerialize(), and ToString().
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

	/**
	 * Our custom serialization for this struct
	 * It makes more sense to do serialization work in here since the engine uses this more than operator<<().
	 */
	virtual bool Serialize(FArchive& InOutArchive) PURE_VIRTUAL(FGCPropertyWrapperBase::Serialize, return false; );

	/** Our custom replication for this struct (we only want to replicate Value) */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) PURE_VIRTUAL(FGCPropertyWrapperBase::NetSerialize, return false; );

	/** Return the ToString() of Value. */
	virtual FString ToString() const PURE_VIRTUAL(FGCPropertyWrapperBase::ToString, return FString(); );

protected:
	/** The pointer to the FProperty on our outer's UClass */
	UPROPERTY(Transient)
		TFieldPath<FProperty> Property;

	/** The pointer to our outer - needed for replication */
	UPROPERTY(Transient)
		TWeakObjectPtr<UObject> PropertyOwner;
};


/**
 * Using this macro with parameters lets us do generic logic in any child class.
 * Boilerplate code for subclasses of FGCPropertyWrapperBase. Include this anywhere in your struct body.
 * Uses your declared Value member.
 * Uses your defined value change delegate type.
 * Assumes you have the WithSerializer type trait.
 * Assumes you have the WithNetSerializer type trait.
 */
#define GC_PROPERTY_WRAPPER_MEMBERS(ValueType, ValueTypeName, DefaultValue) \
private:\
typedef FGC##ValueTypeName##PropertyWrapper TPropertyWrapperType;\
typedef ValueType TValueType;\
\
public:\
\
TPropertyWrapperType()\
	: Value(DefaultValue)\
{ }\
TPropertyWrapperType(UObject* InPropertyOwner, const FName& InPropertyName, const TValueType& InValue = DefaultValue)\
	: FGCPropertyWrapperBase(InPropertyOwner, InPropertyName, GetScriptStruct())\
	, Value(InValue)\
{ }\
\
\
/** Broadcasted whenever Value changes */\
TMulticastDelegate<void(const TValueType&, const TValueType&)> ValueChangeDelegate;\
\
virtual UScriptStruct* GetScriptStruct() const { return StaticStruct(); }\
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
/** Uses our custom serialization */\
friend FArchive& operator<<(FArchive& InOutArchive, TPropertyWrapperType& InOut##ValueTypeName##PropertyWrapper)\
{\
	InOut##ValueTypeName##PropertyWrapper.Serialize(InOutArchive);\
	return InOutArchive;\
}\
\
/** Debug string displaying the property name and value */\
FString GetDebugString(bool bDetailedDebugString = false) const\
{\
	if (bDetailedDebugString)\
	{\
		return PropertyOwner->GetPathName(PropertyOwner->GetTypedOuter<ULevel>()) + TEXT(".") + Property->GetName() + TEXT(": ") + ToString();\
	}\
\
	return Property->GetName() + TEXT(": ") + ToString();\
}
