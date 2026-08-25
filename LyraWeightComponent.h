// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/GameFrameworkComponent.h"
#include "LyraWeightContributor.h"

#include "LyraWeightComponent.generated.h"

#define UE_API LYRAGAME_API

class UGameplayEffect;
class ULyraWeightComponent;
class UAbilitySystemComponent;
class ULyraWeightSet;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWeight_AttributeChanged, ULyraWeightComponent*, WeightComponent, float, OldValue, float, NewValue);

/**
 * ULyraWeightComponent
 * Tracks the current carry weight and maximum carry capacity of an actor.
 */
UCLASS(Blueprintable, Meta = (BlueprintSpawnableComponent))
class ULyraWeightComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

public:

	/** Default constructor. Initializes attributes to their default values. */
	UE_API ULyraWeightComponent(const FObjectInitializer& ObjectInitializer);

	/** Finds the weight component on the specified actor, if any. */
	UFUNCTION(BlueprintPure, Category = "Weight")
	static ULyraWeightComponent* FindWeightComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<ULyraWeightComponent>() : nullptr);
	}

	/** Initializes the weight component with the specified ability system component. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	/** Uninitializes the weight component, removing any subscriptions to the ability system component. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API void UninitializeFromAbilitySystem();
	
	/** Returns the current carry weight. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API float GetCurrentWeight() const;

	/** Returns the maximum carry weight. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API float GetMaxWeight() const;

	/** Returns the current carry weight normalized to the range [0, 1]. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API float GetWeightNormalized() const;

	/** Returns true if the current weight is greater than or equal to the maximum weight. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API bool IsOverweight() const;

	/** Delegate fired when CurrentWeight changes. */
	UPROPERTY(BlueprintAssignable)
	FWeight_AttributeChanged OnCurrentWeightChanged;

	/** Delegate fired when MaxWeight changes. */
	UPROPERTY(BlueprintAssignable)
	FWeight_AttributeChanged OnMaxWeightChanged;

protected:

	/** Called when the component is unregistered from the world. Cleans up any subscriptions to the ability system component. */
	UE_API virtual void OnUnregister() override;
	
	/** */
	UE_API virtual void BeginPlay() override;
 
	/** */
	UE_API void HandleAbilitySystemInitialized();
	
	/** */
	UE_API void HandleAbilitySystemUninitialized();

	/** Evaluates whether the current weight exceeds the maximum weight and applies or removes the overweight gameplay effect as necessary. */
	UE_API void EvaluateOverweightState();

	/** Recalculates the current weight by gathering all contributors and summing their contributions. */
	UFUNCTION(BlueprintCallable, Category = "Weight")
	UE_API void RecalculateWeight();

	/** Finds all contributors on the owner actor and returns them in OutContributors. */
	UE_API void GatherContributors(TArray<TScriptInterface<ILyraWeightContributor>>& OutContributors) const;

	/**  Binds to all contributors on the owner actor, so we can recalculate weight when they change. */
	UE_API void BindContributors();

	/**  Unbinds from all contributors on the owner actor. */
	UE_API void UnbindContributors();

	/** Called when the current weight changes. Broadcasts OnCurrentWeightChanged and evaluates overweight state. */
	UE_API virtual void HandleCurrentWeightChanged(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue);

	/** Called when the max weight changes. Broadcasts OnMaxWeightChanged and evaluates overweight state. */
	UE_API virtual void HandleMaxWeightChanged(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue);

	/** Gameplay effect class to apply when the current weight exceeds the maximum weight. */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> OverweightEffectClass;

	/** Handle of the currently applied overweight gameplay effect, if any. */
	FActiveGameplayEffectHandle OverweightEffectHandle;

	/** Ability system component that owns the weight set. */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Weight set that tracks the current and maximum weight. */
	UPROPERTY()
	TObjectPtr<const ULyraWeightSet> LyraWeightSet;

	/** Weak pointers to contributors we're currently subscribed to, so we can unbind cleanly. */
	TArray<TWeakObjectPtr<UObject>> BoundContributors;

	/** Handles for the delegates we bound to contributors, so we can unbind cleanly. */
	TArray<FDelegateHandle> BoundContributorHandles;
};

#undef UE_API