// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeightComponent.h"
#include "LyraWeightSet.h"
#include "LyraLogChannels.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

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
	LyraWeightSet = nullptr;
}

// -------------------------------------------------------------------
//	Init / Uninit
// -------------------------------------------------------------------

void ULyraWeightComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);
	
	UE_LOG(LogLyra, Warning, TEXT("WeightComp Init on %s, HasAuthority=%d, WeightSet=%s"),
	*GetNameSafe(GetOwner()), GetOwner()->HasAuthority(), *GetNameSafe(LyraWeightSet));

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

	LyraWeightSet = AbilitySystemComponent->GetSet<ULyraWeightSet>();
	if (!LyraWeightSet)
	{
		UE_LOG(LogLyra, Error, TEXT("WeightComponent: Cannot initialize [%s] — ULyraWeightSet not found on ASC."), *GetNameSafe(Owner));
		AbilitySystemComponent = nullptr;
		return;
	}

	LyraWeightSet->OnWeightChanged.AddUObject(this, &ThisClass::HandleCurrentWeightChanged);
	LyraWeightSet->OnMaxWeightChanged.AddUObject(this, &ThisClass::HandleMaxWeightChanged);

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

	if (LyraWeightSet)
	{
		LyraWeightSet->OnWeightChanged.RemoveAll(this);
		LyraWeightSet->OnMaxWeightChanged.RemoveAll(this);
	}

	LyraWeightSet = nullptr;
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

	// Bind to the pawn extension component's ASC signals. Because this component is added by a
	// game feature, the base character never references it; instead we hook the same ASC
	// init/uninit signals the character's built-in components use. _RegisterAndCall fires
	// immediately if the ASC is already initialized, so late-added components don't miss it.
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

	// Collect contributors from a given actor into the output array.
	auto CollectFrom = [&OutContributors](const AActor* Source)
	{
		if (!Source)
		{
			return;
		}

		TArray<UActorComponent*> Components;
		Source->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component && Component->Implements<ULyraWeightContributor>())
			{
				OutContributors.Add(TScriptInterface<ILyraWeightContributor>(Component));
			}
		}
	};

	// The owner pawn (e.g. equipment lives here).
	CollectFrom(Owner);

	// And its controller (e.g. inventory lives here in Lyra).
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		CollectFrom(OwnerPawn->GetController());
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

	UE_LOG(LogLyra, Warning, TEXT("RecalculateWeight: found %d contributors, total %f"), Contributors.Num(), Total);

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
	return LyraWeightSet ? LyraWeightSet->GetWeight() : 0.f;
}

float ULyraWeightComponent::GetMaxWeight() const
{
	return LyraWeightSet ? LyraWeightSet->GetMaxWeight() : 0.f;
}

float ULyraWeightComponent::GetWeightNormalized() const
{
	if (LyraWeightSet)
	{
		const float Max = LyraWeightSet->GetMaxWeight();
		return (Max > 0.f) ? (LyraWeightSet->GetWeight() / Max) : 0.f;
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