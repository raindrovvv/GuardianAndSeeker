// Copyright Greed Fennec Studio. All Rights Reserved.

#include "Character/F_GS_DamageEvent.h"

FGS_DamageEvent::FGS_DamageEvent()
	: FDamageEvent()
	, HitReactType(EHitReactType::DamageOnly)
	, bSuppressCameraEffects(false)
	, bIsCritical(false)
{
}