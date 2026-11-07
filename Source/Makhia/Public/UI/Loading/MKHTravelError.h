// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MKHTravelError.generated.h"

/**
 * Why a match could not start or a travel failed, shown to the player back in the lobby (W_TravelError) with a
 * developer code. Full list and sources: see Documentation/LoadingScreen.md.
 */
UENUM(BlueprintType)
enum class EMKHTravelError : uint8
{
	/** No error. */
	None,
	/** NET-CONNECTION-LOST: the connection with the other machine was closed. */
	ConnectionLost,
	/** NET-CONNECTION-TIMEOUT: the other machine stopped answering. */
	ConnectionTimeout,
	/** NET-CONNECT-FAILED: the connection to the host could not be opened. */
	ConnectFailed,
	/** NET-VERSION-MISMATCH: the two machines run different builds. */
	VersionMismatch,
	/** NET-LISTEN-FAILED: the lobby could not be hosted (listen socket). */
	ListenFailed,
	/** NET-FAILURE: any other network failure. */
	NetworkFailure,
	/** TRAVEL-LOAD-FAILED: the destination level could not be loaded. */
	LoadFailed,
	/** TRAVEL-FAILED: any other travel failure. */
	TravelFailed,
	/** MM-JOIN-FAILED: joining the opponent's lobby failed. */
	JoinFailed,
	/** LOBBY-START-FAILED: the host could not close its lobby to start the match. */
	LobbyStartFailed,
	/** MATCH-OPPONENT-LEFT: the opponent left while the match was loading. */
	OpponentLeft,
	/** MATCH-START-TIMEOUT: the match never started after the opponent was found. */
	MatchStartTimeout,
	/** TRAVEL-TIMEOUT: a travel did not end in time. */
	TravelTimeout
};

/** A travel error with the moment it happened, kept by UMKHLoadingScreenSubsystem until the player saw it. */
USTRUCT(BlueprintType)
struct MAKHIA_API FMKHTravelErrorInfo
{
	GENERATED_BODY()

	/** What went wrong. */
	UPROPERTY(BlueprintReadOnly, Category = "Loading|Error")
	EMKHTravelError Error = EMKHTravelError::None;

	/** Engine detail for the log (failure enum, map, message); never shown to the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Loading|Error")
	FString Detail;

	/** Returns true when an error is set. */
	bool IsSet() const { return Error != EMKHTravelError::None; }
};

/** Codes and player texts of the travel errors. */
UCLASS()
class MAKHIA_API UMKHTravelErrorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the developer code of an error (e.g. "NET-CONNECTION-LOST"), shown as is in every language.
	 *
	 * @param Error  Error to describe.
	 */
	UFUNCTION(BlueprintPure, Category = "Loading|Error")
	static FString GetErrorCode(EMKHTravelError Error);

	/**
	 * Returns the short title of the error dialog.
	 *
	 * @param Error  Error to describe.
	 */
	UFUNCTION(BlueprintPure, Category = "Loading|Error")
	static FText GetErrorTitle(EMKHTravelError Error);

	/**
	 * Returns the sentence that explains the error to the player.
	 *
	 * @param Error  Error to describe.
	 */
	UFUNCTION(BlueprintPure, Category = "Loading|Error")
	static FText GetErrorMessage(EMKHTravelError Error);
};
