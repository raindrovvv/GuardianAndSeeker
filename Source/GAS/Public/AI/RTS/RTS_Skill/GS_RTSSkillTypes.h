#pragma once

#include "CoreMinimal.h"
#include "GS_RTSSkillTypes.generated.h"

UENUM(BlueprintType)
enum class ERTSSkillTargetType : uint8
{
    None        UMETA(DisplayName = "None"),
    Location    UMETA(DisplayName = "Location"),
    Actor       UMETA(DisplayName = "Actor"),
    Direction   UMETA(DisplayName = "Direction")
};
