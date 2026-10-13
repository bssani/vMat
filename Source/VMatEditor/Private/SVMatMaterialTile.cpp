#include "SVMatMaterialTile.h"

#include "AssetThumbnail.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "VMatUI.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SVMatMaterialTile"

namespace VMatTile
{
	constexpr float TilePadding = 4.f;

	static const FButtonStyle& TileStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FStyleColors::Secondary, 6.f))
			.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Hover, 6.f, FStyleColors::Select, 1.f))
			.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Header, 6.f, FStyleColors::Primary, 1.f))
			.SetDisabled(FSlateRoundedBoxBrush(FStyleColors::Panel, 6.f))
			.SetNormalForeground(FStyleColors::Foreground)
			.SetHoveredForeground(FStyleColors::ForegroundHover)
			.SetPressedForeground(FStyleColors::ForegroundHover)
			.SetDisabledForeground(FStyleColors::Foreground)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	// 타일 오른쪽 위에 뜨는 동그란 X. 올리면 빨개져
	static const FButtonStyle& RemoveButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FStyleColors::Panel, 8.f))
			.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Error, 8.f))
			.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Error, 8.f))
			.SetNormalForeground(FStyleColors::Foreground)
			.SetHoveredForeground(FStyleColors::White)
			.SetPressedForeground(FStyleColors::White)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}
}

void SVMatMaterialTile::Construct(const FArguments& InArgs)
{
	Thumbnail = InArgs._Thumbnail;
	OnApply = InArgs._OnApply;
	OnRemove = InArgs._OnRemove;
	OnBrowse = InArgs._OnBrowse;
	bMissing = !Thumbnail.IsValid();

	const float ThumbnailSize = InArgs._ThumbnailSize;
	const FText ToolTip = bMissing
		? FText::Format(LOCTEXT("MissingTileTooltip", "{0}\nCan't find this material. It may have been moved or deleted."), InArgs._Path)
		: FText::Format(LOCTEXT("TileTooltip", "{0}\n{1}"), InArgs._Path, InArgs._ApplyHint);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SButton)
			.ButtonStyle(&VMatTile::TileStyle())
			.ContentPadding(VMatTile::TilePadding)
			.IsEnabled(!bMissing)
			.ToolTipText(ToolTip)
			.OnClicked_Lambda([this]
			{
				OnApply.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(ThumbnailSize)
					.HeightOverride(ThumbnailSize)
					[
						bMissing ? MakeMissingContent() : Thumbnail->MakeThumbnailWidget()
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.f, 4.f, 2.f, 1.f)
				[
					SNew(SBox)
					.WidthOverride(ThumbnailSize - 4.f)
					[
						SNew(STextBlock)
						.Text(InArgs._Name)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.Justification(ETextJustify::Center)
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(7.f)
		[
			SNew(SButton)
			.ButtonStyle(&VMatTile::RemoveButtonStyle())
			.ContentPadding(3.f)
			.ToolTipText(LOCTEXT("RemoveTooltip", "Remove from this preset (the material asset stays)"))
			.Visibility(this, &SVMatMaterialTile::GetRemoveButtonVisibility)
			.OnClicked_Lambda([this]
			{
				OnRemove.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.X"))
				.DesiredSizeOverride(FVector2D(10.f, 10.f))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

FReply SVMatMaterialTile::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return MouseEvent.GetEffectingButton() == EKeys::RightMouseButton ? FReply::Handled() : FReply::Unhandled();
}

FReply SVMatMaterialTile::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
	{
		return FReply::Unhandled();
	}

	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(
		LOCTEXT("MenuApply", "Apply"),
		LOCTEXT("MenuApplyTooltip", "Apply this material"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Check"),
		FUIAction(FExecuteAction::CreateLambda([this] { OnApply.ExecuteIfBound(); }), FCanExecuteAction::CreateLambda([this] { return !bMissing; })));
	Menu.AddMenuEntry(
		LOCTEXT("MenuBrowse", "Browse to asset"),
		LOCTEXT("MenuBrowseTooltip", "Show this material in the Content Browser"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.BrowseContent"),
		FUIAction(FExecuteAction::CreateLambda([this] { OnBrowse.ExecuteIfBound(); }), FCanExecuteAction::CreateLambda([this] { return !bMissing; })));
	if (OnRemove.IsBound())
	{
		Menu.AddSeparator();
		Menu.AddMenuEntry(
			LOCTEXT("MenuRemove", "Remove from preset"),
			LOCTEXT("MenuRemoveTooltip", "Remove from this preset. The material asset stays."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(FExecuteAction::CreateLambda([this] { OnRemove.ExecuteIfBound(); })));
	}

	const FWidgetPath WidgetPath = MouseEvent.GetEventPath() ? *MouseEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, Menu.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	return FReply::Handled();
}

TSharedRef<SWidget> SVMatMaterialTile::MakeMissingContent() const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ContentBrowser.AssetTileItem.ThumbnailAreaBackground"))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Warning"))
				.ColorAndOpacity(FStyleColors::Warning)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				VMatUI::HintText(LOCTEXT("MissingMaterial", "Missing"))
			]
		];
}

EVisibility SVMatMaterialTile::GetRemoveButtonVisibility() const
{
	if (!OnRemove.IsBound())
	{
		return EVisibility::Collapsed;
	}
	// 못 찾는 material은 정리하라고 X를 항상 보여줘
	return bMissing || IsHovered() ? EVisibility::Visible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
