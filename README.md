# Lyra Weight System

A modular carry-weight system for Lyra, built on GAS and Modular Gameplay. It tracks how much a pawn is carrying against a maximum capacity and applies a gameplay effect when the pawn is overweight. The core knows nothing about inventory, items, or equipment — anything that adds weight declares itself through a small interface, so the system composes with any number of sources without being edited.

Designed to be attached through a Game Feature, so weight only exists in the modes that want it (for example a survival experience) and never touches the base character.

## What's inside

| Class | Role |
|---|---|
| `ULyraWeightSet` | Attribute set holding `Weight` and `MaxWeight`. |
| `ULyraWeightComponent` | Gathers contributors, sums their weight, writes the total, and applies the overweight effect. |
| `ILyraWeightContributor` | Interface implemented by anything that adds weight. |

The weight component discovers every `ILyraWeightContributor` on its owner, subscribes to each one's change delegate, and sums their contributions into the `Weight` attribute. It never references a concrete contributor type, so adding a new source of weight requires no change to the component.

## Requirements

Lyra (the module depends on `LyraGame`), plus the `GameplayAbilities` and `ModularGameplay` engine plugins, which are enabled by default in every Lyra project.

## Setup

1. Add `ULyraWeightSet` to an ability set (`ULyraAbilitySet`) under its granted attributes. The Game Feature that grants this ability set adds the weight set to the pawn's ability system, the same way other Lyra attribute sets are granted.
2. Add `ULyraWeightComponent` to the pawn. The recommended path is a Game Feature Action (Add Components) targeting `ALyraCharacter`, so weight is layered on only for the experiences that enable the feature.
3. Assign an overweight gameplay effect on the component's `OverweightEffectClass`. The component applies this effect while `Weight` exceeds `MaxWeight` and removes it when `Weight` drops back under.

No manual initialization call is needed. The component binds to the pawn extension component's ability-system signals and initializes itself once the ASC is ready — even when the component is added late by a Game Feature.

## Adding a weight source

Implement `ILyraWeightContributor` on any component whose contents should count toward carry weight (an inventory, an equipment manager, a backpack). Two things are required:

- `GetWeightContribution()` — return this source's current total weight.
- `GetOnWeightContributionChanged()` — return a delegate you broadcast whenever that weight may have changed.

The weight component sums `GetWeightContribution()` across every contributor on the pawn and recalculates whenever any contributor broadcasts its change delegate. A minimal C++ implementation:

```cpp
// In the header: implement the interface and hold the delegate.
class UMyInventoryComponent : public UActorComponent, public ILyraWeightContributor
{
    GENERATED_BODY()

public:
    virtual float GetWeightContribution() const override;
    virtual FOnWeightContributionChanged& GetOnWeightContributionChanged() override
    {
        return OnWeightContributionChanged;
    }

private:
    FOnWeightContributionChanged OnWeightContributionChanged;
};
```

```cpp
// In the cpp: return the total, and broadcast wherever contents change (server side).
float UMyInventoryComponent::GetWeightContribution() const
{
    // Sum the weight of whatever this component holds.
    return /* your total */ 0.f;
}

// Call this from the authoritative add/remove path:
//   OnWeightContributionChanged.Broadcast();
```

Weight is resolved on the server: the component writes the total on the authority, and the `Weight` attribute replicates to clients through GAS. Clients read the replicated value for UI; they do not recompute it.

## Reading the weight

`ULyraWeightComponent` exposes `GetCurrentWeight()`, `GetMaxWeight()`, `GetWeightNormalized()` (0–1, useful for a progress bar), and `IsOverweight()`. It also broadcasts `OnCurrentWeightChanged` and `OnMaxWeightChanged` for UI binding.

## Note on the overweight effect

Currently the component applies `OverweightEffectClass` directly while over capacity. A later revision may invert this: the component would only add and remove a `Status.Overweight` gameplay tag, and the effect (plus any ability that reacts to being overweight, such as a movement-speed penalty) would key off that tag instead. That keeps the state (the tag) separate from the behaviour (whatever reacts to it), so multiple systems can respond to being overweight without the component knowing about any of them.