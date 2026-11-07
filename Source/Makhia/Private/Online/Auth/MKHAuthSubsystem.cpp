// Copyright (c) 2025 Luigi Pennisi. All rights reserved.

#include "Online/Auth/MKHAuthSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "MKHLogChannels.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Online/Auth/MKHAuthProvider.h"
#include "Online/Auth/MKHAuthSaveGame.h"
#include "Online/Backend/MKHOnlineBackend.h"
#include "Online/Backend/MKHOnlineServicesSubsystem.h"
#include "Online/Backend/Null/MKHAuthProvider_Null.h"
#include "Online/MKHOnlineSettings.h"
#include "TimerManager.h"

namespace MKHAuth
{
	/** Save slot holding the login preferences. */
	static const TCHAR* SaveSlotName = TEXT("Auth");

	/** User index of the save slot. Makhia has a single local player. */
	static constexpr int32 SaveUserIndex = 0;

	/** Guest name length bounds, counted on the trimmed name. */
	static constexpr int32 GuestNameMinLength = 3;
	static constexpr int32 GuestNameMaxLength = 16;

	/** Command-line switch of the development start-up login (-AuthToken=<credential name>). */
	static const TCHAR* DevCredentialSwitch = TEXT("AuthToken=");

	/** Returns true for the characters allowed in a guest name (EIK sends the name to EOS as ANSI). */
	static bool IsGuestNameCharacter(TCHAR Character)
	{
		return (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT(' ') || Character == TEXT('_') || Character == TEXT('-');
	}
}

#define LOCTEXT_NAMESPACE "MKHAuth"

// ============================================================
// Lifecycle
// ============================================================

void UMKHAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMKHOnlineServicesSubsystem>();

	LoadSaveData();
	CreateProvider();
}

void UMKHAuthSubsystem::Deinitialize()
{
	ClearAutoLoginTimer();
	DestroyProvider();

	Super::Deinitialize();
}

// ============================================================
// Public Interface
// ============================================================

UMKHAuthSubsystem* UMKHAuthSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHAuthSubsystem>() : nullptr;
}

void UMKHAuthSubsystem::TryAutoLogin()
{
	if (bAutoLoginAttempted)
	{
		return;
	}
	bAutoLoginAttempted = true;

	FMKHAuthParams Params;
	const EMKHAuthMethod Method = ResolveAutoLoginMethod(Params);
	if (Method == EMKHAuthMethod::None)
	{
		SetState(EMKHAuthState::Offline);
		return;
	}

	UE_LOG(LogMKHAuth, Log, TEXT("Start-up login with method %s (timeout %.1f s)."), *UEnum::GetValueAsString(Method),
		UMKHOnlineSettings::Get()->AutoLoginTimeout);
	StartLogin(Method, Params, true);
}

bool UMKHAuthSubsystem::Login(EMKHAuthMethod Method, const FMKHAuthParams& Params)
{
	return StartLogin(Method, Params, false);
}

bool UMKHAuthSubsystem::LoginWithPlatformAccount()
{
	return Login(EMKHAuthMethod::PlatformAccount, FMKHAuthParams());
}

bool UMKHAuthSubsystem::LoginAsGuest(const FString& DisplayName)
{
	FMKHAuthParams Params;
	Params.GuestDisplayName = DisplayName;
	return Login(EMKHAuthMethod::Guest, Params);
}

bool UMKHAuthSubsystem::LoginWithDevProfile(int32 ProfileIndex)
{
	const TArray<FString>& Names = UMKHOnlineSettings::Get()->DevCredentialNames;
	FMKHAuthParams Params;
	Params.DevCredentialName = Names.IsValidIndex(ProfileIndex) ? Names[ProfileIndex] : FString();
	return Login(EMKHAuthMethod::Developer, Params);
}

void UMKHAuthSubsystem::Logout()
{
	if (State != EMKHAuthState::LoggedIn)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Logout ignored: not signed in."));
		return;
	}

	UE_LOG(LogMKHAuth, Log, TEXT("Logout requested: the start-up login is disabled until the next sign-in."));
	if (IsValid(SaveData))
	{
		SaveData->bAutoLogin = false;
		WriteSaveData();
	}

	// Offline first: the listeners leave their online session while the backend still knows the player.
	User = FMKHAuthUser();
	LostMethod = EMKHAuthMethod::None;
	bBackendLogoutPending = true;
	SetState(EMKHAuthState::Offline);
	TryFinishBackendLogout();
}

bool UMKHAuthSubsystem::SwitchToPlatformAccount()
{
	if (State != EMKHAuthState::LoggedIn || User.Method != EMKHAuthMethod::Guest || !SupportsMethod(EMKHAuthMethod::PlatformAccount))
	{
		UE_LOG(LogMKHAuth, Log, TEXT("SwitchToPlatformAccount ignored: not signed in as a guest, or no platform account."));
		return false;
	}

	UE_LOG(LogMKHAuth, Log, TEXT("Switching from guest to the platform account."));
	bPlatformLoginAfterLogout = true;
	Logout();
	return true;
}

void UMKHAuthSubsystem::AddLogoutHold()
{
	++LogoutHoldCount;
}

void UMKHAuthSubsystem::ReleaseLogoutHold()
{
	LogoutHoldCount = FMath::Max(LogoutHoldCount - 1, 0);
	TryFinishBackendLogout();
}

bool UMKHAuthSubsystem::CanStartLogin() const
{
	return IsOnlineReachable() && State == EMKHAuthState::Offline && !bBackendLogoutPending
		&& (!Provider->IsBusy() || IsWaitingForTimedOutLogin());
}

bool UMKHAuthSubsystem::IsOnlineAvailable() const
{
	return IsValid(Provider) && Provider->IsBackendAvailable();
}

FText UMKHAuthSubsystem::GetOnlineUnavailableText() const
{
	return IsValid(Provider) ? Provider->GetUnavailableText() : GetAuthErrorText(EMKHAuthError::BackendUnavailable);
}

bool UMKHAuthSubsystem::IsOnlineReachable() const
{
	return IsOnlineAvailable() && Provider->IsBackendReachable();
}

bool UMKHAuthSubsystem::CanSignOut() const
{
	return State == EMKHAuthState::LoggedIn && IsValid(Provider) && Provider->SupportsSignOut();
}

bool UMKHAuthSubsystem::HasSignInChoice() const
{
	const bool bDevProfiles = ShouldShowDevLoginButtons() && SupportsMethod(EMKHAuthMethod::Developer);
	return SupportsMethod(EMKHAuthMethod::Guest) || bDevProfiles;
}

bool UMKHAuthSubsystem::SupportsMethod(EMKHAuthMethod Method) const
{
	if (Method == EMKHAuthMethod::Developer && UE_BUILD_SHIPPING)
	{
		return false;
	}
	return IsValid(Provider) && Provider->SupportsMethod(Method);
}

FText UMKHAuthSubsystem::GetMethodDisplayName(EMKHAuthMethod Method) const
{
	return IsValid(Provider) ? Provider->GetMethodDisplayName(Method) : FText::GetEmpty();
}

FString UMKHAuthSubsystem::GetSavedGuestName() const
{
	return IsValid(SaveData) ? SaveData->GuestDisplayName : FString();
}

EMKHGuestNameError UMKHAuthSubsystem::ValidateGuestName(const FString& Name, FString& OutTrimmedName)
{
	OutTrimmedName = Name.TrimStartAndEnd();
	if (OutTrimmedName.Len() < MKHAuth::GuestNameMinLength)
	{
		return EMKHGuestNameError::TooShort;
	}
	if (OutTrimmedName.Len() > MKHAuth::GuestNameMaxLength)
	{
		return EMKHGuestNameError::TooLong;
	}

	for (const TCHAR Character : OutTrimmedName)
	{
		if (!MKHAuth::IsGuestNameCharacter(Character))
		{
			return EMKHGuestNameError::InvalidCharacter;
		}
	}
	return EMKHGuestNameError::None;
}

int32 UMKHAuthSubsystem::GetGuestNameMaxLength()
{
	return MKHAuth::GuestNameMaxLength;
}

FText UMKHAuthSubsystem::GetGuestNameErrorText(EMKHGuestNameError Error)
{
	switch (Error)
	{
	case EMKHGuestNameError::TooShort:
		return FText::Format(LOCTEXT("GuestNameTooShort", "The name must be at least {Min} characters long."),
			FText::AsNumber(MKHAuth::GuestNameMinLength));
	case EMKHGuestNameError::TooLong:
		return FText::Format(LOCTEXT("GuestNameTooLong", "The name can be at most {Max} characters long."),
			FText::AsNumber(MKHAuth::GuestNameMaxLength));
	case EMKHGuestNameError::InvalidCharacter:
		return LOCTEXT("GuestNameInvalidCharacter", "Use only letters, numbers, spaces, _ and -.");
	default:
		return FText::GetEmpty();
	}
}

FText UMKHAuthSubsystem::GetAuthErrorText(EMKHAuthError Error)
{
	switch (Error)
	{
	case EMKHAuthError::BackendUnavailable:
		return LOCTEXT("ErrorBackendUnavailable", "Online services are not available.");
	case EMKHAuthError::NoNetwork:
		return LOCTEXT("ErrorNoNetwork", "Could not reach the online services. Check your connection.");
	case EMKHAuthError::Cancelled:
		return LOCTEXT("ErrorCancelled", "Sign-in cancelled.");
	case EMKHAuthError::InvalidCredentials:
		return LOCTEXT("ErrorInvalidCredentials", "The account could not be verified. Please sign in again.");
	case EMKHAuthError::InvalidName:
		return LOCTEXT("ErrorInvalidName", "Choose a valid name.");
	case EMKHAuthError::MethodNotSupported:
		return LOCTEXT("ErrorMethodNotSupported", "This sign-in method is not available.");
	case EMKHAuthError::Busy:
		return LOCTEXT("ErrorBusy", "A sign-in is already in progress.");
	case EMKHAuthError::Timeout:
		return LOCTEXT("ErrorTimeout", "The online services did not answer in time.");
	case EMKHAuthError::Unknown:
		return LOCTEXT("ErrorUnknown", "Sign-in failed. Please try again.");
	default:
		return FText::GetEmpty();
	}
}

bool UMKHAuthSubsystem::ShouldShowDevLoginButtons()
{
	return UMKHOnlineSettings::Get()->ShouldShowDevLoginButtons();
}

TArray<FString> UMKHAuthSubsystem::GetDevCredentialNames()
{
	return UMKHOnlineSettings::Get()->DevCredentialNames;
}

// ============================================================
// Protected / Internal Logic - Provider and saved preferences
// ============================================================

void UMKHAuthSubsystem::CreateProvider()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UMKHOnlineServicesSubsystem* Services = IsValid(GameInstance) ? GameInstance->GetSubsystem<UMKHOnlineServicesSubsystem>() : nullptr;
	const UMKHOnlineBackend* Backend = IsValid(Services) ? Services->GetBackend() : nullptr;
	TSubclassOf<UMKHAuthProvider> ProviderClass = IsValid(Backend) ? Backend->GetAuthProviderClass() : nullptr;
	if (!IsValid(ProviderClass) || ProviderClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogMKHAuth, Warning, TEXT("The online backend has no auth provider, using Null."));
		ProviderClass = UMKHAuthProvider_Null::StaticClass();
	}

	Provider = NewObject<UMKHAuthProvider>(this, ProviderClass);
	Provider->OnIdentityLost.AddUObject(this, &UMKHAuthSubsystem::HandleProviderIdentityLost);
	Provider->OnIdentityRestored.AddUObject(this, &UMKHAuthSubsystem::HandleProviderIdentityRestored);
	Provider->InitializeProvider();
	UE_LOG(LogMKHAuth, Log, TEXT("Auth provider %s ready (online available: %d)."), *ProviderClass->GetName(), IsOnlineAvailable() ? 1 : 0);
}

void UMKHAuthSubsystem::DestroyProvider()
{
	if (IsValid(Provider))
	{
		Provider->OnIdentityLost.RemoveAll(this);
		Provider->OnIdentityRestored.RemoveAll(this);
		Provider->ShutdownProvider();
	}
	Provider = nullptr;
}

void UMKHAuthSubsystem::LoadSaveData()
{
	if (UGameplayStatics::DoesSaveGameExist(MKHAuth::SaveSlotName, MKHAuth::SaveUserIndex))
	{
		SaveData = Cast<UMKHAuthSaveGame>(UGameplayStatics::LoadGameFromSlot(MKHAuth::SaveSlotName, MKHAuth::SaveUserIndex));
	}

	if (!IsValid(SaveData))
	{
		SaveData = Cast<UMKHAuthSaveGame>(UGameplayStatics::CreateSaveGameObject(UMKHAuthSaveGame::StaticClass()));
	}
}

void UMKHAuthSubsystem::WriteSaveData() const
{
	if (!IsValid(SaveData) || !UGameplayStatics::SaveGameToSlot(SaveData, MKHAuth::SaveSlotName, MKHAuth::SaveUserIndex))
	{
		UE_LOG(LogMKHAuth, Warning, TEXT("Could not write the login preferences to the '%s' save slot."), MKHAuth::SaveSlotName);
	}
}

EMKHAuthMethod UMKHAuthSubsystem::ResolveAutoLoginMethod(FMKHAuthParams& OutParams) const
{
	if (IsPlayInEditor())
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Start-up login skipped in Play In Editor (platform logins need Standalone Game)."));
		return EMKHAuthMethod::None;
	}

	if (TryGetCommandLineDevCredential(OutParams.DevCredentialName))
	{
		return EMKHAuthMethod::Developer;
	}

	if (!UMKHOnlineSettings::Get()->bAutoLoginOnBoot)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Start-up login disabled in the project settings, starting offline."));
		return EMKHAuthMethod::None;
	}

	bool bExplicitLogout = false;
	const EMKHAuthMethod SavedMethod = GetSavedAutoLoginMethod(bExplicitLogout);
	if (SavedMethod != EMKHAuthMethod::None)
	{
		OutParams.GuestDisplayName = SaveData->GuestDisplayName;
		return SavedMethod;
	}

	// A platform session that exists outside the game (Steam client) is used from the very first start-up.
	if (!bExplicitLogout && IsValid(Provider) && Provider->SignsInByDefault() && SupportsMethod(EMKHAuthMethod::PlatformAccount))
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Start-up login: signing in the platform account of the running client."));
		return EMKHAuthMethod::PlatformAccount;
	}

	UE_LOG(LogMKHAuth, Log, TEXT("Start-up login: nothing to retry%s, starting offline."), bExplicitLogout ? TEXT(" (signed out by the player)") : TEXT(""));
	return EMKHAuthMethod::None;
}

EMKHAuthMethod UMKHAuthSubsystem::GetSavedAutoLoginMethod(bool& OutExplicitLogout) const
{
	OutExplicitLogout = false;
	if (!IsValid(SaveData) || SaveData->LastMethod == EMKHAuthMethod::None)
	{
		return EMKHAuthMethod::None;
	}
	if (!SaveData->bAutoLogin)
	{
		OutExplicitLogout = true;
		return EMKHAuthMethod::None;
	}

	// A method saved with another backend (e.g. a guest login of the EOS days) is not retried.
	const EMKHAuthMethod Method = SaveData->LastMethod;
	const bool bRetryable = (Method == EMKHAuthMethod::PlatformAccount || Method == EMKHAuthMethod::Guest) && SupportsMethod(Method);
	return bRetryable ? Method : EMKHAuthMethod::None;
}

bool UMKHAuthSubsystem::TryGetCommandLineDevCredential(FString& OutCredentialName)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Value(FCommandLine::Get(), MKHAuth::DevCredentialSwitch, OutCredentialName) && !OutCredentialName.IsEmpty();
#endif
}

bool UMKHAuthSubsystem::IsPlayInEditor() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const FWorldContext* WorldContext = IsValid(GameInstance) ? GameInstance->GetWorldContext() : nullptr;
	return WorldContext && WorldContext->WorldType == EWorldType::PIE;
}

// ============================================================
// Protected / Internal Logic - Login flow
// ============================================================

EMKHAuthError UMKHAuthSubsystem::CheckLoginAllowed(EMKHAuthMethod Method, const FMKHAuthParams& Params, FMKHAuthParams& OutParams) const
{
	OutParams = Params;
	if (!IsOnlineAvailable())
	{
		return EMKHAuthError::BackendUnavailable;
	}
	const bool bProviderBusy = Provider->IsBusy() && !IsWaitingForTimedOutLogin();
	if (State == EMKHAuthState::LoggingIn || State == EMKHAuthState::LoggedIn || bBackendLogoutPending || bProviderBusy)
	{
		return EMKHAuthError::Busy;
	}
	if (!SupportsMethod(Method))
	{
		return EMKHAuthError::MethodNotSupported;
	}
	if (Method == EMKHAuthMethod::Guest && ValidateGuestName(Params.GuestDisplayName, OutParams.GuestDisplayName) != EMKHGuestNameError::None)
	{
		return EMKHAuthError::InvalidName;
	}
	if (Method == EMKHAuthMethod::Developer && Params.DevCredentialName.IsEmpty())
	{
		return EMKHAuthError::InvalidCredentials;
	}
	return EMKHAuthError::None;
}

bool UMKHAuthSubsystem::StartLogin(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent)
{
	FMKHAuthParams ProviderParams;
	const EMKHAuthError Refusal = CheckLoginAllowed(Method, Params, ProviderParams);
	if (Refusal != EMKHAuthError::None)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Login %s refused: %s."), *UEnum::GetValueAsString(Method), *UEnum::GetValueAsString(Refusal));
		FailLogin(Method, Refusal, !bSilent);
		return false;
	}

	// Without network EOS can take tens of seconds to give up on the start-up login.
	if (IsWaitingForTimedOutLogin())
	{
		QueueLogin(Method, ProviderParams);
		return true;
	}

	BeginProviderLogin(Method, ProviderParams, bSilent);
	return true;
}

void UMKHAuthSubsystem::BeginProviderLogin(EMKHAuthMethod Method, const FMKHAuthParams& Params, bool bSilent)
{
	++LoginRequestId;
	PendingMethod = Method;
	PendingParams = Params;
	bPendingSilent = bSilent;
	bPendingTimedOut = false;
	SetState(EMKHAuthState::LoggingIn);

	if (bSilent)
	{
		AutoLoginDeadline = FPlatformTime::Seconds() + FMath::Max(UMKHOnlineSettings::Get()->AutoLoginTimeout, 1.f);
		AutoLoginTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMKHAuthSubsystem::HandleAutoLoginTimeout));
	}

	Provider->Login(Method, Params, bSilent,
		FMKHAuthLoginCompleteDelegate::CreateUObject(this, &UMKHAuthSubsystem::HandleProviderLoginComplete, LoginRequestId));
}

bool UMKHAuthSubsystem::IsWaitingForTimedOutLogin() const
{
	return bPendingTimedOut && IsValid(Provider) && Provider->IsBusy();
}

void UMKHAuthSubsystem::QueueLogin(EMKHAuthMethod Method, const FMKHAuthParams& Params)
{
	UE_LOG(LogMKHAuth, Log, TEXT("Login %s queued: the timed-out start-up login has not answered yet."), *UEnum::GetValueAsString(Method));
	bHasQueuedLogin = true;
	QueuedMethod = Method;
	QueuedParams = Params;
	SetState(EMKHAuthState::LoggingIn);
}

void UMKHAuthSubsystem::RunQueuedLogin(const FMKHAuthLoginResult& TimedOutResult)
{
	const EMKHAuthMethod Method = QueuedMethod;
	const FMKHAuthParams Params = QueuedParams;
	bHasQueuedLogin = false;
	QueuedMethod = EMKHAuthMethod::None;
	QueuedParams = FMKHAuthParams();

	if (TimedOutResult.bSuccess && IsSameLogin(PendingMethod, PendingParams, Method, Params))
	{
		CompleteLoginSuccess(TimedOutResult.User);
		return;
	}

	if (TimedOutResult.bSuccess)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("The start-up login signed in another identity: signing it out before the queued login."));
		Provider->Logout(FSimpleDelegate::CreateWeakLambda(this, [this, Method, Params]()
		{
			BeginProviderLogin(Method, Params, false);
		}));
		return;
	}

	UE_LOG(LogMKHAuth, Log, TEXT("The start-up login failed (%s): starting the queued login."), *TimedOutResult.RawError);
	BeginProviderLogin(Method, Params, false);
}

bool UMKHAuthSubsystem::IsSameLogin(EMKHAuthMethod MethodA, const FMKHAuthParams& ParamsA, EMKHAuthMethod MethodB, const FMKHAuthParams& ParamsB)
{
	if (MethodA != MethodB)
	{
		return false;
	}

	switch (MethodA)
	{
	case EMKHAuthMethod::Guest:
		return ParamsA.GuestDisplayName.Equals(ParamsB.GuestDisplayName, ESearchCase::CaseSensitive);
	case EMKHAuthMethod::Developer:
		return ParamsA.DevCredentialName == ParamsB.DevCredentialName;
	default:
		return true;
	}
}

void UMKHAuthSubsystem::HandleProviderLoginComplete(const FMKHAuthLoginResult& Result, uint32 RequestId)
{
	if (RequestId != LoginRequestId)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Ignoring the result of an outdated login."));
		return;
	}

	ClearAutoLoginTimer();

	// A start-up login that outlived its timeout is still adopted: the player just comes online late.
	const bool bStillWanted = State == EMKHAuthState::LoggingIn || (bPendingTimedOut && State == EMKHAuthState::Offline);
	bPendingTimedOut = false;
	if (!bStillWanted)
	{
		UE_LOG(LogMKHAuth, Log, TEXT("Ignoring a login result that arrived after the state moved on."));
		return;
	}

	if (bHasQueuedLogin)
	{
		RunQueuedLogin(Result);
		return;
	}

	if (Result.bSuccess)
	{
		CompleteLoginSuccess(Result.User);
		return;
	}

	UE_LOG(LogMKHAuth, Warning, TEXT("Login %s failed: %s (%s)."), *UEnum::GetValueAsString(PendingMethod),
		*UEnum::GetValueAsString(Result.Error), *Result.RawError);
	FailLogin(PendingMethod, Result.Error, !bPendingSilent);
}

void UMKHAuthSubsystem::CompleteLoginSuccess(const FMKHAuthUser& SignedInUser)
{
	User = SignedInUser;
	LastError = EMKHAuthError::None;
	LostMethod = EMKHAuthMethod::None;
	UE_LOG(LogMKHAuth, Log, TEXT("Signed in as '%s' (%s, user %s)."), *User.DisplayName, *UEnum::GetValueAsString(User.Method), *User.UserId);

	if (IsValid(SaveData))
	{
		SaveData->LastMethod = User.Method;
		SaveData->bAutoLogin = true;
		if (User.Method == EMKHAuthMethod::Guest)
		{
			SaveData->GuestDisplayName = User.DisplayName;
		}
		WriteSaveData();
	}

	SetState(EMKHAuthState::LoggedIn);
}

void UMKHAuthSubsystem::FailLogin(EMKHAuthMethod Method, EMKHAuthError Error, bool bNotify)
{
	LastError = Error;
	if (Error == EMKHAuthError::NoNetwork && LostMethod == EMKHAuthMethod::None)
	{
		// Like a lost identity: the provider reports the network coming back and the same login runs again.
		LostMethod = Method;
		LostParams = PendingParams;
	}
	if (State != EMKHAuthState::LoggedIn)
	{
		SetState(EMKHAuthState::Offline);
	}

	if (bNotify)
	{
		OnLoginFailed.Broadcast(Method, Error);
	}
}

bool UMKHAuthSubsystem::HandleAutoLoginTimeout(float DeltaTime)
{
	// Wall clock: engine delta times skip the start-up and loading stalls.
	if (FPlatformTime::Seconds() < AutoLoginDeadline)
	{
		return true;
	}

	AutoLoginTimeoutHandle.Reset();
	if (State != EMKHAuthState::LoggingIn || !bPendingSilent)
	{
		return false;
	}

	UE_LOG(LogMKHAuth, Warning, TEXT("Start-up login timed out: going on offline (a late answer is still adopted)."));
	bPendingTimedOut = true;
	LastError = EMKHAuthError::Timeout;
	SetState(EMKHAuthState::Offline);
	return false;
}

void UMKHAuthSubsystem::HandleProviderIdentityLost()
{
	if (State != EMKHAuthState::LoggedIn)
	{
		return;
	}

	// Not an explicit logout: the saved preferences keep the start-up login, and a reconnection signs back in.
	UE_LOG(LogMKHAuth, Warning, TEXT("The backend signed the player out: going offline until it is reachable again."));
	LostMethod = User.Method;
	LostParams = FMKHAuthParams();
	LostParams.GuestDisplayName = User.Method == EMKHAuthMethod::Guest ? User.DisplayName : FString();
	User = FMKHAuthUser();
	SetState(EMKHAuthState::Offline);
}

void UMKHAuthSubsystem::HandleProviderIdentityRestored()
{
	if (LostMethod == EMKHAuthMethod::None || State != EMKHAuthState::Offline || bBackendLogoutPending)
	{
		return;
	}

	// A failed attempt keeps LostMethod: the next reconnection tries again.
	UE_LOG(LogMKHAuth, Log, TEXT("The backend is reachable again: signing back in (%s)."), *UEnum::GetValueAsString(LostMethod));
	StartLogin(LostMethod, LostParams, true);
}

void UMKHAuthSubsystem::TryFinishBackendLogout()
{
	if (!bBackendLogoutPending || LogoutHoldCount > 0)
	{
		return;
	}

	bBackendLogoutPending = false;
	if (IsValid(Provider))
	{
		Provider->Logout(FSimpleDelegate::CreateUObject(this, &UMKHAuthSubsystem::HandleProviderLogoutComplete));
	}
}

void UMKHAuthSubsystem::HandleProviderLogoutComplete()
{
	UE_LOG(LogMKHAuth, Log, TEXT("Logout completed."));

	if (bPlatformLoginAfterLogout)
	{
		bPlatformLoginAfterLogout = false;
		LoginWithPlatformAccount();
	}
}

void UMKHAuthSubsystem::SetState(EMKHAuthState NewState)
{
	if (State == NewState)
	{
		return;
	}

	const EMKHAuthState OldState = State;
	State = NewState;
	UE_LOG(LogMKHAuth, Log, TEXT("Auth state: %s -> %s."), *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState));

	OnAuthStateChangedNative.Broadcast(NewState, OldState);
	OnAuthStateChanged.Broadcast(NewState, OldState);
}

void UMKHAuthSubsystem::ClearAutoLoginTimer()
{
	if (AutoLoginTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AutoLoginTimeoutHandle);
		AutoLoginTimeoutHandle.Reset();
	}
}

// ============================================================
// Development console commands (not in Shipping)
// ============================================================

#if !UE_BUILD_SHIPPING
namespace MKHAuthConsole
{
	/** Runs Action on the auth subsystem of the world the command was typed in. */
	static void WithAuth(UWorld* World, TFunctionRef<void(UMKHAuthSubsystem&)> Action)
	{
		if (UMKHAuthSubsystem* Auth = UMKHAuthSubsystem::Get(World); IsValid(Auth))
		{
			Action(*Auth);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs LoginGuestCommand(TEXT("MKH.Auth.LoginGuest"),
		TEXT("Signs in as a guest: MKH.Auth.LoginGuest <Name>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithAuth(World, [&Args](UMKHAuthSubsystem& Auth) { Auth.LoginAsGuest(FString::Join(Args, TEXT(" "))); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs LoginPlatformCommand(TEXT("MKH.Auth.LoginPlatform"),
		TEXT("Signs in with the platform account of the online backend: MKH.Auth.LoginPlatform"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithAuth(World, [](UMKHAuthSubsystem& Auth) { Auth.LoginWithPlatformAccount(); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs LoginDevCommand(TEXT("MKH.Auth.LoginDev"),
		TEXT("Signs in with a Dev Auth Tool profile: MKH.Auth.LoginDev <ProfileIndex>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 ProfileIndex = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
			WithAuth(World, [ProfileIndex](UMKHAuthSubsystem& Auth) { Auth.LoginWithDevProfile(ProfileIndex); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs LogoutCommand(TEXT("MKH.Auth.Logout"),
		TEXT("Signs out and disables the start-up login: MKH.Auth.Logout [DelaySeconds]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			WithAuth(World, [Delay](UMKHAuthSubsystem& Auth)
			{
				if (Delay <= 0.f)
				{
					Auth.Logout();
					return;
				}

				// The delay lets -ExecCmds sign out once the start-up login is done.
				FTimerHandle Handle;
				Auth.GetGameInstance()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(&Auth, &UMKHAuthSubsystem::Logout), Delay, false);
			});
		}));

	static FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("MKH.Auth.Status"),
		TEXT("Logs the sign-in state: MKH.Auth.Status"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithAuth(World, [](UMKHAuthSubsystem& Auth)
			{
				const FMKHAuthUser User = Auth.GetUser();
				UE_LOG(LogMKHAuth, Display, TEXT("Auth status: %s, user '%s' (%s), online available %d, last error %s."),
					*UEnum::GetValueAsString(Auth.GetAuthState()), *User.DisplayName, *UEnum::GetValueAsString(User.Method),
					Auth.IsOnlineAvailable() ? 1 : 0, *UEnum::GetValueAsString(Auth.GetLastError()));
			});
		}));
}
#endif

#undef LOCTEXT_NAMESPACE
