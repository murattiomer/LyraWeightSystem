# Lyra Weight System

[English](README.md) | [Türkçe](README.tr.md)

A modular carry-weight system for Lyra, built on GAS and Modular Gameplay. It tracks how much a pawn is carrying against a maximum capacity and applies a gameplay effect when the pawn is overweight.

The core knows nothing about inventory, items, or equipment. Anything that adds weight declares itself through a small interface, so the system composes with any number of sources without being edited. It is designed to be attached through a Game Feature, so weight only exists in the experiences that want it and never touches the base character.

## What's inside

| Class | Role |
|---|---|
| `ULyraWeightSet` | Attribute set holding `Weight` and `MaxWeight`. |
| `ULyraWeightComponent` | Gathers contributors, sums their weight, writes the total, and applies the overweight effect. |
| `ILyraWeightContributor` | Interface implemented by anything that adds weight. |

## How it fits together

The weight component discovers every `ILyraWeightContributor` on its owner, subscribes to each one's change delegate, and sums their contributions into the `Weight` attribute. It never references a concrete contributor type, so adding a new source of weight requires no change to the component. Weight is resolved on the server and the attribute replicates to clients for UI.

![Runtime data flow](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_8.png)

The system initializes itself. A Game Feature grants an ability set (which adds the weight set to the ASC) and adds the weight component; when the pawn extension component reports the ASC is ready, the component binds its contributors and computes the first total.

![Initialization sequence](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_7.png)

## Requirements

Lyra (the module lives under `Source/LyraGame/WeightSystem`), plus the `GameplayAbilities` and `ModularGameplay` plugins, which are enabled by default in every Lyra project.

## Setup

### 1. Grant the weight set through an ability set

Add `ULyraWeightSet` to an ability set (`ULyraAbilitySet`) under **Granted Attributes**. The Game Feature that grants this ability set adds the weight set to the pawn's ability system, the same way every other Lyra attribute set is granted.

![Weight set in an ability set](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_1.png)

### 2. Add the weight component

Add `ULyraWeightComponent` to the pawn through a Game Feature Action (Add Components) targeting `ALyraCharacter`. This keeps weight layered on only for the experiences that enable the feature. No manual initialization call is needed.

![Adding the weight component](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_6.png)

### 3. Assign the overweight effect

Set `OverweightEffectClass` on the component. The component applies this effect while `Weight` exceeds `MaxWeight` and removes it when `Weight` drops back under. The effect grants the `Status.Overweight` tag and directly modifies the movement speed attribute (`LyraMovementSet.MovementSpeed`).

![Overweight effect grants tag and modifies movement speed](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_2.png)

## Adding a weight source

Implement `ILyraWeightContributor` on any component whose contents should count toward carry weight — an inventory, an equipment manager, a backpack. The weight component sums `GetWeightContribution()` across every contributor on the pawn and recalculates whenever any contributor broadcasts its change delegate.

First, implement the interface and hold the delegate. In the header:

```cpp
/**
 * Manages an inventory
 */
UCLASS(MinimalAPI, BlueprintType)
class ULyraInventoryManagerComponent : public UActorComponent, public ILyraWeightContributor
{
    GENERATED_BODY()

public:
    virtual float GetWeightContribution() const override;
    virtual FOnWeightContributionChanged& GetOnWeightContributionChanged() override;

private:
    UPROPERTY(Replicated)
    FLyraInventoryList InventoryList;

    FOnWeightContributionChanged OnWeightContributionChangedDelegate;
};
```

Then return the current total and hand back the delegate. In the cpp:

```cpp
float ULyraInventoryManagerComponent::GetWeightContribution() const
{
    float TotalWeight = 0.f;

    for (const FLyraInventoryEntry& Entry : InventoryList.Entries)
    {
        if (Entry.Instance)
        {
            // Test: each item counts as 10 weight per stack.
            TotalWeight += 10.f * Entry.StackCount;
        }
    }

    return TotalWeight;
}

FOnWeightContributionChanged& ULyraInventoryManagerComponent::GetOnWeightContributionChanged()
{
    return OnWeightContributionChangedDelegate;
}
```

Finally, broadcast `OnWeightContributionChanged` from every authoritative path that changes those contents, so the weight component knows to recalculate:

```cpp
ULyraInventoryItemInstance* ULyraInventoryManagerComponent::AddItemDefinition(TSubclassOf<ULyraInventoryItemDefinition> ItemDef, int32 StackCount)
{
    ULyraInventoryItemInstance* Result = nullptr;
    if (ItemDef != nullptr)
    {
        Result = InventoryList.AddEntry(ItemDef, StackCount);

        if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && Result)
        {
            AddReplicatedSubObject(Result);
        }

        // Server-side, wherever contents change:
        OnWeightContributionChangedDelegate.Broadcast();
    }
    return Result;
}

void ULyraInventoryManagerComponent::AddItemInstance(ULyraInventoryItemInstance* ItemInstance)
{
    InventoryList.AddEntry(ItemInstance);
    if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
    {
        AddReplicatedSubObject(ItemInstance);

        // Server-side, wherever contents change:
        OnWeightContributionChangedDelegate.Broadcast();
    }
}

void ULyraInventoryManagerComponent::RemoveItemInstance(ULyraInventoryItemInstance* ItemInstance)
{
    InventoryList.RemoveEntry(ItemInstance);

    if (ItemInstance && IsUsingRegisteredSubObjectList())
    {
        RemoveReplicatedSubObject(ItemInstance);

        // Server-side, wherever contents change:
        OnWeightContributionChangedDelegate.Broadcast();
    }
}
```

Weight is resolved on the server: the component writes the total on the authority, and the `Weight` attribute replicates to clients through GAS. Clients read the replicated value for UI; they do not recompute it.

## Reading the weight

`ULyraWeightComponent` exposes `GetCurrentWeight()`, `GetMaxWeight()`, `GetWeightNormalized()` (0–1, for a progress bar), and `IsOverweight()`. It also broadcasts `OnCurrentWeightChanged` and `OnMaxWeightChanged`, so UI can update the moment weight changes instead of polling every frame.

A HUD widget binds to possession changes to safely manage these delegates. By listening to `OnPossessedPawnChanged`, the widget can find the current weight component, unbind from the previous pawn's component, and bind its custom events to the new pawn's `OnCurrentWeightChanged` delegate. 

![Widget binds to possessed pawn changes](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_11.png)

The UI refresh logic reads the component directly when the delegate fires. `GetWeightNormalized()` drives the progress bar, `GetCurrentWeight()` and `GetMaxWeight()` format the text label, and `IsOverweight()` is used to select the UI color:

![Widget refresh reads the component](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_12.png)

### Runtime Result

When the pawn spawns, the initial weight is calculated from the starting inventory (e.g., 10/100):

![In-game HUD displaying initial weight](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_3.png)

Picking up an item or weapon triggers the change delegate, recalculating the total weight and updating the UI in real time (e.g., 20/100):

![In-game HUD displaying updated weight after pickup](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_4.png)