// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "UI/Loading/MKHTravelError.h"

#define LOCTEXT_NAMESPACE "MKHTravelError"

FString UMKHTravelErrorLibrary::GetErrorCode(const EMKHTravelError Error)
{
	switch (Error)
	{
	case EMKHTravelError::ConnectionLost:    return TEXT("NET-CONNECTION-LOST");
	case EMKHTravelError::ConnectionTimeout: return TEXT("NET-CONNECTION-TIMEOUT");
	case EMKHTravelError::ConnectFailed:     return TEXT("NET-CONNECT-FAILED");
	case EMKHTravelError::VersionMismatch:   return TEXT("NET-VERSION-MISMATCH");
	case EMKHTravelError::ListenFailed:      return TEXT("NET-LISTEN-FAILED");
	case EMKHTravelError::NetworkFailure:    return TEXT("NET-FAILURE");
	case EMKHTravelError::LoadFailed:        return TEXT("TRAVEL-LOAD-FAILED");
	case EMKHTravelError::TravelFailed:      return TEXT("TRAVEL-FAILED");
	case EMKHTravelError::JoinFailed:        return TEXT("MM-JOIN-FAILED");
	case EMKHTravelError::LobbyStartFailed:  return TEXT("LOBBY-START-FAILED");
	case EMKHTravelError::OpponentLeft:      return TEXT("MATCH-OPPONENT-LEFT");
	case EMKHTravelError::MatchStartTimeout: return TEXT("MATCH-START-TIMEOUT");
	case EMKHTravelError::TravelTimeout:     return TEXT("TRAVEL-TIMEOUT");
	default:                                 return FString();
	}
}

FText UMKHTravelErrorLibrary::GetErrorTitle(const EMKHTravelError Error)
{
	switch (Error)
	{
	case EMKHTravelError::ConnectionLost:
	case EMKHTravelError::ConnectionTimeout:
	case EMKHTravelError::NetworkFailure:
		return LOCTEXT("TitleConnectionLost", "Connection lost");
	case EMKHTravelError::ConnectFailed:
	case EMKHTravelError::VersionMismatch:
	case EMKHTravelError::JoinFailed:
		return LOCTEXT("TitleCouldNotJoin", "Couldn't join the match");
	case EMKHTravelError::ListenFailed:
		return LOCTEXT("TitleOffline", "You are playing offline");
	case EMKHTravelError::OpponentLeft:
		return LOCTEXT("TitleOpponentLeft", "Your opponent left");
	default:
		return LOCTEXT("TitleMatchNotStarted", "The match couldn't start");
	}
}

FText UMKHTravelErrorLibrary::GetErrorMessage(const EMKHTravelError Error)
{
	switch (Error)
	{
	case EMKHTravelError::ConnectionLost:
		return LOCTEXT("MsgConnectionLost", "The connection with your opponent was lost. You are back in your lobby.");
	case EMKHTravelError::ConnectionTimeout:
		return LOCTEXT("MsgConnectionTimeout", "Your opponent stopped responding. Check your connection and try again.");
	case EMKHTravelError::ConnectFailed:
		return LOCTEXT("MsgConnectFailed", "Couldn't connect to your opponent. Try searching again.");
	case EMKHTravelError::VersionMismatch:
		return LOCTEXT("MsgVersionMismatch", "Your opponent has a different version of the game. Make sure both of you have the latest update.");
	case EMKHTravelError::ListenFailed:
		return LOCTEXT("MsgListenFailed", "Your lobby couldn't be opened online. You can still play offline.");
	case EMKHTravelError::LoadFailed:
		return LOCTEXT("MsgLoadFailed", "The arena couldn't be loaded. Try again; if it keeps happening, verify the game files.");
	case EMKHTravelError::JoinFailed:
		return LOCTEXT("MsgJoinFailed", "Couldn't join your opponent's lobby. Try searching again.");
	case EMKHTravelError::OpponentLeft:
		return LOCTEXT("MsgOpponentLeft", "Your opponent left before the match started.");
	case EMKHTravelError::MatchStartTimeout:
		return LOCTEXT("MsgMatchStartTimeout", "The match took too long to start. Try searching again.");
	default:
		return LOCTEXT("MsgGeneric", "Something went wrong while starting the match. Try again.");
	}
}

#undef LOCTEXT_NAMESPACE
