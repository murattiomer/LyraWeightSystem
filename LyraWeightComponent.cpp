// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeightComponent.h"
#include "LyraWeightSet.h"
#include "LyraLogChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeightComponent)

// -------------------------------------------------------------------
//	Constructor
// -------------------------------------------------------------------

ULyraWeightComponent::ULyraWeightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
	WeightSet = nullptr;
}

// -------------------------------------------------------------------
//	Init / Uninit
// -------------------------------------------------------------------

void ULyraWeightComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (AbilitySystemComponent)
	{
		UE_LOG(LogLyra, Error, TEXT("WeightComponent: [%s] already initialized with an ability system."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogLyra, Error, TEXT("WeightComponent: Cannot initialize [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}

	WeightSet = AbilitySystemComponent->GetSet<ULyraWeightSet>();
	if (!WeightSet)
	{
		UE_LOG(LogLyra, Error, TEXT("WeightComponent: Cannot initialize [%s] — UWeightSet not found on ASC."), *GetNameSafe(Owner));
		AbilitySystemComponent = nullptr;
		return;
	}

	WeightSet->OnWeightChanged.AddUObject(this, &ThisClass::HandleCurrentWeightChanged);
	WeightSet->OnMaxWeightChanged.AddUObject(this, &ThisClass::HandleMaxWeightChanged);

	// Subscribe to contributors and compute the initial total.
	BindContributors();
	RecalculateWeight();

	// Seed listeners with the current values so UI bound at init has something to show.
	OnCurrentWeightChanged.Broadcast(this, GetCurrentWeight(), GetCurrentWeight());
	OnMaxWeightChanged.Broadcast(this, GetMaxWeight(), GetMaxWeight());

	EvaluateOverweightState();
}

void ULyraWeightComponent::UninitializeFromAbilitySystem()
{
	if (AbilitySystemComponent && OverweightEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(OverweightEffectHandle);
	}
	OverweightEffectHandle.Invalidate();

	UnbindContributors();

	if (WeightSet)
	{
		WeightSet->OnWeightChanged.RemoveAll(this);
		WeightSet->OnMaxWeightChanged.RemoveAll(this);
	}

	WeightSet = nullptr;
	AbilitySystemComponent = nullptr;
}

void ULyraWeightComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();
	Super::OnUnregister();
}

void ULyraWeightComponent::BeginPlay()
{
	Super::BeginPlay();
 
	if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExtComp->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
 
		PawnExtComp->OnAbilitySystemUninitialized_Register(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
	}
	else
	{
		UE_LOG(LogLyra, Error, TEXT("LyraWeightComponent on [%s]: no LyraPawnExtensionComponent found; cannot initialize."), *GetNameSafe(GetOwner()));
	}
}

void ULyraWeightComponent::HandleAbilitySystemInitialized()
{
	if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		if (ULyraAbilitySystemComponent* LyraASC = PawnExtComp->GetLyraAbilitySystemComponent())
		{
			InitializeWithAbilitySystem(LyraASC);
		}
	}
}
 
void ULyraWeightComponent::HandleAbilitySystemUninitialized()
{
	UninitializeFromAbilitySystem();
}

// -------------------------------------------------------------------
//	Contributor discovery
// -------------------------------------------------------------------

void ULyraWeightComponent::GatherContributors(TArray<TScriptInterface<ILyraWeightContributor>>& OutContributors) const
{
	OutContributors.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component && Component->Implements<ULyraWeightContributor>())
		{
			OutContributors.Add(TScriptInterface<ILyraWeightContributor>(Component));
		}
	}
}

void ULyraWeightComponent::RecalculateWeight()
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	TArray<TScriptInterface<ILyraWeightContributor>> Contributors;
	GatherContributors(Contributors);

	float Total = 0.f;
	for (const TScriptInterface<ILyraWeightContributor>& Contributor : Contributors)
	{
		if (Contributor)
		{
			Total += Contributor->GetWeightContribution();
		}
	}

	AbilitySystemComponent->SetNumericAttributeBase(ULyraWeightSet::GetWeightAttribute(), Total);
}

void ULyraWeightComponent::BindContributors()
{
	UnbindContributors();

	TArray<TScriptInterface<ILyraWeightContributor>> Contributors;
	GatherContributors(Contributors);

	for (const TScriptInterface<ILyraWeightContributor>& Contributor : Contributors)
	{
		UObject* Object = Contributor.GetObject();
		if (!Object)
		{
			continue;
		}

		FDelegateHandle Handle = Contributor->GetOnWeightContributionChanged().AddUObject(
			this, &ThisClass::RecalculateWeight);

		BoundContributors.Add(Object);
		BoundContributorHandles.Add(Handle);
	}
}

void ULyraWeightComponent::UnbindContributors()
{
	for (int32 Index = 0; Index < BoundContributors.Num(); ++Index)
	{
		UObject* Object = BoundContributors[Index].Get();
		if (Object)
		{
			if (ILyraWeightContributor* Contributor = Cast<ILyraWeightContributor>(Object))
			{
				Contributor->GetOnWeightContributionChanged().Remove(BoundContributorHandles[Index]);
			}
		}
	}

	BoundContributors.Reset();
	BoundContributorHandles.Reset();
}

// -------------------------------------------------------------------
//	Getters
// -------------------------------------------------------------------

float ULyraWeightComponent::GetCurrentWeight() const
{
	return WeightSet ? WeightSet->GetWeight() : 0.f;
}

float ULyraWeightComponent::GetMaxWeight() const
{
	return WeightSet ? WeightSet->GetMaxWeight() : 0.f;
}

float ULyraWeightComponent::GetWeightNormalized() const
{
	if (WeightSet)
	{
		const float Max = WeightSet->GetMaxWeight();
		return (Max > 0.f) ? (WeightSet->GetWeight() / Max) : 0.f;
	}
	return 0.f;
}

bool ULyraWeightComponent::IsOverweight() const
{
	return OverweightEffectHandle.IsValid();
}

// -------------------------------------------------------------------
//	Overweight evaluation
// -------------------------------------------------------------------

void ULyraWeightComponent::EvaluateOverweightState()
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	const float Current = GetCurrentWeight();
	const float Max = GetMaxWeight();
	const bool bOverweight = (Current > Max) && (Max > 0.f);
	const bool bHasEffect = OverweightEffectHandle.IsValid();

	if (bOverweight && !bHasEffect)
	{
		if (OverweightEffectClass)
		{
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddInstigator(GetOwner(), GetOwner());

			const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
				OverweightEffectClass, 1.f, Context);

			if (Spec.IsValid())
			{
				OverweightEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}
	else if (!bOverweight && bHasEffect)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(OverweightEffectHandle);
		OverweightEffectHandle.Invalidate();
	}
}

// -------------------------------------------------------------------
//	AttributeSet Handlers
// -------------------------------------------------------------------

void ULyraWeightComponent::HandleCurrentWeightChanged(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue)
{
	OnCurrentWeightChanged.Broadcast(this, OldValue, NewValue);

	EvaluateOverweightState();
}

void ULyraWeightComponent::HandleMaxWeightChanged(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue)
{
	OnMaxWeightChanged.Broadcast(this, OldValue, NewValue);

	EvaluateOverweightState();
}