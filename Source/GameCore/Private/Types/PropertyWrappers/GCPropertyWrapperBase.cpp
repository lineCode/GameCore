// Fill out your copyright notice in the Description page of Project Settings.


#include "Types/PropertyWrappers/GCPropertyWrapperBase.h"



FGCPropertyWrapperBase::FGCPropertyWrapperBase()
	: bMarkNetDirtyOnChange(false)
	, Property(nullptr)
	, PropertyOwner(nullptr)
{
}
