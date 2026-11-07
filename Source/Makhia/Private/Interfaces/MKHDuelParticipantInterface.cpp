// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Interfaces/MKHDuelParticipantInterface.h"

// ============================================================
// UMKHDuelParticipantLibrary
// ============================================================

FText UMKHDuelParticipantLibrary::GetParticipantDisplayName(const AActor* Participant)
{
	const IMKHDuelParticipant* DuelParticipant = Cast<IMKHDuelParticipant>(Participant);
	return DuelParticipant ? DuelParticipant->GetParticipantDisplayName() : FText::GetEmpty();
}

bool UMKHDuelParticipantLibrary::IsHumanParticipant(const AActor* Participant)
{
	const IMKHDuelParticipant* DuelParticipant = Cast<IMKHDuelParticipant>(Participant);
	return DuelParticipant && DuelParticipant->IsHumanParticipant();
}
