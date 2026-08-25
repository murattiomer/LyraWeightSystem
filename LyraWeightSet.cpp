// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeightSet.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeightSet)

ULyraWeightSet::ULyraWeightSet()
{
	InitWeight(0.f);
	InitMaxWeight(100.f);
}

void ULyraWeightSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ULyraWeightSet, Weight, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULyraWeightSet, MaxWeight, COND_None, REPNOTIFY_Always);
}

// -------------------------------------------------------------------
//	RepNotify
// -------------------------------------------------------------------

void ULyraWeightSet::OnRep_Weight(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULyraWeightSet, Weight, OldValue);
}

void ULyraWeightSet::OnRep_MaxWeight(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULyraWeightSet, MaxWeight, OldValue);
}

// -------------------------------------------------------------------
//	Pre/Post Execute
// -------------------------------------------------------------------

void ULyraWeightSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetWeightAttribute())
	{
		OnWeightChanged.Broadcast(nullptr, nullptr, nullptr, NewValue - OldValue, OldValue, NewValue);
	}
	else if (Attribute == GetMaxWeightAttribute())
	{
		OnMaxWeightChanged.Broadcast(nullptr, nullptr, nullptr, NewValue - OldValue, OldValue, NewValue);
	}
}

// -------------------------------------------------------------------
//	Attribute Clamping
// -------------------------------------------------------------------

void ULyraWeightSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetWeightAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxWeightAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void ULyraWeightSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetWeightAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetMaxWeightAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}