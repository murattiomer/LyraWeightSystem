// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "LyraWeightContributor.generated.h"


DECLARE_MULTICAST_DELEGATE(FOnWeightContributionChanged);

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class ULyraWeightContributor : public UInterface
{
	GENERATED_BODY()
};

/**
 * ILyraWeightContributor
 * Interface for objects that contribute to the player's weight.
 */
class ILyraWeightContributor
{
	GENERATED_BODY()

public:

	// Returns this contributor's current weight contribution.
	virtual float GetWeightContribution() const = 0;

	// Returns the delegate broadcast when this contributor's weight may have changed.
	virtual FOnWeightContributionChanged& GetOnWeightContributionChanged() = 0;
};