// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/MKHSystemWidget.h"
#include "UI/WidgetControllers/WidgetController.h"

void UMKHSystemWidget::SetWidgetController(UWidgetController* InWidgetController)
{
	WidgetController = InWidgetController;
	OnWidgetControllerSet();
}

UWidgetController* UMKHSystemWidget::GetWidgetController() const
{
	return WidgetController;
}
