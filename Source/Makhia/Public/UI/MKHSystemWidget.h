// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MKHSystemWidget.generated.h"

class UWidgetController;
/**
 * 
 */
UCLASS()
class MAKHIA_API UMKHSystemWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetWidgetController(UWidgetController* InWidgetController);

	UFUNCTION(BlueprintImplementableEvent)
	void OnWidgetControllerSet();

protected:

	/** Gets the widget controller assigned by the HUD (null until SetWidgetController is called). */
	UWidgetController* GetWidgetController() const;

private:

	UPROPERTY(BlueprintReadOnly, meta = (AllowprivateAccess = true))
	TObjectPtr<UWidgetController> WidgetController;
	
};
