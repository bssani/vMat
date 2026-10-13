#include "VMatUI.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace VMatUI
{
	TSharedRef<SWidget> IconButton(FName IconName, const FText& ToolTip, FOnClicked OnClicked, TAttribute<bool> IsEnabled)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(4.f))
			.ToolTipText(ToolTip)
			.IsEnabled(IsEnabled)
			.OnClicked(OnClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(IconName))
				.ColorAndOpacity(FSlateColor::UseForeground())
			];
	}

	TSharedRef<SWidget> TextButton(FName IconName, const FText& Label, const FText& ToolTip, FOnClicked OnClicked, TAttribute<bool> IsEnabled)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(6.f, 2.f))
			.ToolTipText(ToolTip)
			.IsEnabled(IsEnabled)
			.OnClicked(OnClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(IconName))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
					.Text(Label)
				]
			];
	}

	TSharedRef<STextBlock> HintText(TAttribute<FText> Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
}
