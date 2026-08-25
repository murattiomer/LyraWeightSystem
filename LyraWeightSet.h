// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "LyraWeightSet.generated.h"

/**
 * UWeightSet
 * Tracks the player's current carry weight and maximum carry capacity.
 */
UCLASS(BlueprintType)
class LYRAGAME_API ULyraWeightSet : public ULyraAttributeSet
{
	GENERATED_BODY()

public:
	/** Default constructor. Initializes attributes to their default values. */
	ULyraWeightSet();

	/** Current weight of the player. */
	ATTRIBUTE_ACCESSORS(ULyraWeightSet, Weight);
	
	/** Maximum weight the player can carry. */
	ATTRIBUTE_ACCESSORS(ULyraWeightSet, MaxWeight);
	
	/** Initializes the Weight attribute to the specified value. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Delegate fired when CurrentWeight changes.
	mutable FLyraAttributeEvent OnWeightChanged;

	// Delegate fired when MaxWeight changes.
	mutable FLyraAttributeEvent OnMaxWeightChanged;

protected:
	/** Called after an attribute has changed. */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	/** Called before an attribute's base value is changed. Used to clamp values. */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/** Called before an attribute's value is changed. Used to clamp values. */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

private:

	/** RepNotify for Weight attribute. */
	UFUNCTION()
	void OnRep_Weight(const FGameplayAttributeData& OldValue);

	/** RepNotify for MaxWeight attribute. */
	UFUNCTION()
	void OnRep_MaxWeight(const FGameplayAttributeData& OldValue);

	/** Current weight of the player. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Weight, Category = "Weight", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Weight;

	/** Maximum weight the player can carry. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxWeight, Category = "Weight", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxWeight;
};