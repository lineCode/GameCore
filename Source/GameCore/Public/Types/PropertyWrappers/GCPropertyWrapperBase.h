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
 * See GC_PROPERTY_WRAPPER_BODY for boilerplate code and subclass requirements.
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

	FProperty* GetSelfPropertyPointer() const { return SelfPropertyPointer.Get(); }
	UObject* GetOwner() const { return PropertyOwner.Get(); }
	FName GetPropertyName() const { return SelfPropertyPointer->GetFName(); }

	/**
	 * Our custom serialization for this struct
	 * It makes more sense to do serialization work in here since the engine uses this more than operator<<().
	 */
	virtual bool Serialize(FArchive& InOutArchive) PURE_VIRTUAL(FGCPropertyWrapperBase::Serialize, return false; );

	/** Our custom replication for this struct (we only want to replicate Value) */
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) PURE_VIRTUAL(FGCPropertyWrapperBase::NetSerialize, return false; );

	/** Return the ToString() of Value. */
	virtual FString ToString() const PURE_VIRTUAL(FGCPropertyWrapperBase::ToString, return FString(); );

protected:
	/** The pointer to the FProperty on our outer's UClass */
	UPROPERTY(Transient)
		TFieldPath<FProperty> SelfPropertyPointer;

	/** The pointer to our outer - needed for replication */
	UPROPERTY(Transient)
		TWeakObjectPtr<UObject> PropertyOwner;


	/** The pointer to the Value FProperty on our subclass */
	UPROPERTY(Transient)
		TFieldPath<FProperty> ValueProperty;
};


/**
 * Using this macro with parameters lets us do generic logic in any child class.
 * Boilerplate code for subclasses of FGCPropertyWrapperBase. Include this anywhere in your struct body.
 * Uses your declared Value member.
 * Uses your defined value change delegate type.
 * Assumes you have the WithSerializer type trait.
 * Assumes you have the WithNetSerializer type trait.
 */
#define GC_PROPERTY_WRAPPER_BODY(PropertyWrapperType, ValueType, DefaultValue) \
private:\
void InitializeValueProperty()\
{\
	ValueProperty = FindFProperty<FProperty>(StaticStruct(), GET_MEMBER_NAME_CHECKED(PropertyWrapperType, Value));\
}\
\
public:\
PropertyWrapperType()\
	: Value(DefaultValue)\
{\
	InitializeValueProperty();\
}\
PropertyWrapperType(UObject* InPropertyOwner, const FName& InPropertyName, const ValueType& InValue = DefaultValue)\
	: FGCPropertyWrapperBase(InPropertyOwner, InPropertyName, GetScriptStruct())\
	, Value(InValue)\
{\
	InitializeValueProperty();\
}\
\
/** Broadcasted whenever Value changes */\
TMulticastDelegate<void(const ValueType&, const ValueType&)> ValueChangeDelegate;\
\
virtual UScriptStruct* GetScriptStruct() const { return StaticStruct(); }\
\
/** Implements implicit conversion from this struct to Value's type. Allows you to treat this struct as its Value's type in code */\
operator ValueType() const\
{\
	return Value;\
}\
\
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
}\
\
/** Uses our custom serialization */\
friend FArchive& operator<<(FArchive& InOutArchive, PropertyWrapperType& InOutPropertyWrapper)\
{\
	InOutPropertyWrapper.Serialize(InOutArchive);\
	return InOutArchive;\
}\
\
virtual bool Serialize(FArchive& Ar) override\
{\
	if (Ar.IsSaving())\
	{\
		ValueProperty->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), &Value);\
	}\
\
	if (Ar.IsLoading())\
	{\
		ValueType NewValue;\
		ValueProperty->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), &NewValue);\
		operator=(NewValue);\
	}\
\
	return true;\
}\
\
virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override\
{\
	bool bSuccess = true;\
\
	if (Ar.IsSaving())\
	{\
		bSuccess = ValueProperty->NetSerializeItem(Ar, Map, &Value);\
	}\
\
	if (Ar.IsLoading())\
	{\
		ValueType NewValue;\
		bSuccess = ValueProperty->NetSerializeItem(Ar, Map, &NewValue);\
		operator=(NewValue);\
	}\
\
	return bSuccess;\
}\
\
/** Debug string displaying our name and value */\
FString GetDebugString(bool bDetailedDebugString = false) const\
{\
	if (bDetailedDebugString)\
	{\
		return PropertyOwner->GetPathName(PropertyOwner->GetTypedOuter<ULevel>()) + TEXT(".") + SelfPropertyPointer->GetName() + TEXT(": ") + ToString();\
	}\
\
	return SelfPropertyPointer->GetName() + TEXT(": ") + ToString();\
}