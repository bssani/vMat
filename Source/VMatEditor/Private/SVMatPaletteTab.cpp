#include "SVMatPaletteTab.h"

#include "AssetThumbnail.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/MessageDialog.h"
#include "SAssetDropTarget.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/StyleColors.h"
#include "SVMatMaterialTile.h"
#include "VMatMaterialApplier.h"
#include "VMatPresetSettings.h"
#include "VMatUI.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SVMatPaletteTab"

namespace VMatPalette
{
	constexpr int32 ThumbnailSize = 80;
}

void SVMatPaletteTab::Construct(const FArguments& InArgs)
{
	OnShowMultiSlotMeshes = InArgs._OnShowMultiSlotMeshes;
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);

	// 처음 켜면 preset이 하나도 없어서 기본거 하나 만들어둬
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	if (Settings.Presets.IsEmpty())
	{
		Settings.Presets.AddDefaulted_GetRef().Name = TEXT("Default");
		Settings.Save();
	}
	CurrentPresetName = Settings.FindPreset(Settings.LastPresetName) ? Settings.LastPresetName : Settings.Presets[0].Name;

	RefreshPresetOptions();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakePresetBar()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			MakeMaterialArea()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSelectionFooter()
		]
	];

	RebuildMaterialGrid();
	RefreshSelectionSummary();

	// 예전처럼 0.1초마다 확인하지 말고 선택 바뀔때만 다시 계산해
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddSP(this, &SVMatPaletteTab::OnSelectionChanged);
}

SVMatPaletteTab::~SVMatPaletteTab()
{
	USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
}

TSharedRef<SWidget> SVMatPaletteTab::MakePresetBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				// 평소엔 드롭다운, 이름 바꿀때만 입력칸으로 바뀌어
				SAssignNew(PresetNameSwitcher, SWidgetSwitcher)
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(PresetCombo, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&PresetNameOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
					{
						const FVMatPreset* Preset = UVMatPresetSettings::Get().FindPreset(*Item);
						return SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString(*Item))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(12.f, 0.f, 0.f, 0.f)
							[
								VMatUI::HintText(FText::AsNumber(Preset ? Preset->Materials.Num() : 0))
							];
					})
					.OnSelectionChanged(this, &SVMatPaletteTab::OnPresetSelected)
					[
						SNew(STextBlock)
						.Text(this, &SVMatPaletteTab::GetCurrentPresetText)
					]
				]
				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(PresetNameBox, SEditableTextBox)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.ToolTipText(LOCTEXT("RenameBoxTooltip", "Enter to rename, Esc to cancel"))
					.OnTextCommitted(this, &SVMatPaletteTab::OnPresetNameCommitted)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 0.f, 0.f)
			[
				VMatUI::IconButton("Icons.Edit", LOCTEXT("RenamePresetTooltip", "Rename preset"), FOnClicked::CreateSP(this, &SVMatPaletteTab::OnRenamePresetClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				VMatUI::IconButton("Icons.Plus", LOCTEXT("NewPresetTooltip", "New empty preset"), FOnClicked::CreateSP(this, &SVMatPaletteTab::OnNewPresetClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				VMatUI::IconButton("Icons.Duplicate", LOCTEXT("DuplicatePresetTooltip", "Duplicate preset"), FOnClicked::CreateSP(this, &SVMatPaletteTab::OnDuplicatePresetClicked))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				VMatUI::IconButton("Icons.Delete", LOCTEXT("DeletePresetTooltip", "Delete preset"), FOnClicked::CreateSP(this, &SVMatPaletteTab::OnDeletePresetClicked))
			]
		];
}

TSharedRef<SWidget> SVMatPaletteTab::MakeMaterialArea()
{
	// 콘텐츠 브라우저에서 material 끌어다 놓으면 바로 들어가
	return SNew(SAssetDropTarget)
		.bSupportsMultiDrop(true)
		.OnAreAssetsAcceptableForDrop(this, &SVMatPaletteTab::AreAssetsAcceptable)
		.OnAssetsDropped(this, &SVMatPaletteTab::OnAssetsDropped)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
			.Padding(0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10.f, 8.f, 8.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						VMatUI::HintText(TAttribute<FText>::CreateLambda([this]
						{
							const int32 Num = GetCurrentPreset().Materials.Num();
							return Num == 0
								? LOCTEXT("NoMaterialsCount", "No materials")
								: FText::Format(LOCTEXT("MaterialsCount", "{0} {0}|plural(one=material,other=materials)  ·  click one to apply"), Num);
						}))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						VMatUI::TextButton("Icons.Plus", LOCTEXT("AddSelected", "Add selected"),
							LOCTEXT("AddSelectedTooltip", "Add the materials selected in the Content Browser.\nYou can also drag materials onto this area."),
							FOnClicked::CreateSP(this, &SVMatPaletteTab::OnAddSelectedClicked))
					]
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						.Padding(8.f, 4.f, 8.f, 8.f)
						[
							SAssignNew(MaterialGrid, SWrapBox)
							.UseAllottedSize(true)
							.InnerSlotPadding(FVector2D(6.f, 6.f))
						]
					]
					// 비어있을때 뭘 해야하는지 알려주는 안내
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(16.f)
					[
						SNew(SVerticalBox)
						.Visibility_Lambda([this] { return HasMaterials() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(SImage)
							.Image(FSlateIconFinder::FindIconBrushForClass(UMaterial::StaticClass()))
							.DesiredSizeOverride(FVector2D(32.f, 32.f))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.f, 10.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EmptyTitle", "This preset is empty"))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EmptyHint", "Drag materials here from the Content Browser,\nor select them there and click Add selected."))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Justification(ETextJustify::Center)
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SVMatPaletteTab::MakeSelectionFooter()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(10.f, 8.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.SelectInViewport"))
					.ColorAndOpacity_Lambda([this]
					{
						return NumMeshesToChange > 0 ? FStyleColors::Primary : FSlateColor::UseSubduedForeground();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]
					{
						if (NumSelectedActors == 0)
						{
							return LOCTEXT("NoSelection", "Select mesh actors in the level, then click a material.");
						}
						return FText::Format(LOCTEXT("SelectionSummary", "{0} {0}|plural(one=actor,other=actors) selected  ·  {1} {1}|plural(one=mesh,other=meshes) will change"),
							NumSelectedActors, NumMeshesToChange);
					})
				]
			]
			// 건너뛰는거 있으면 경고 한줄. multi-slot이면 그 탭으로 바로 가는 링크
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this] { return SkippedText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Warning"))
					.ColorAndOpacity(FStyleColors::Warning)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this] { return SkippedText; })
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SHyperlink)
					.Text(LOCTEXT("ShowMultiSlot", "Show multi-slot"))
					.ToolTipText(LOCTEXT("ShowMultiSlotTooltip", "Open the Multi-slot tab with these meshes selected"))
					.Visibility_Lambda([this] { return SkippedMultiSlotMeshes.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.OnNavigate(this, &SVMatPaletteTab::OnShowMultiSlotClicked)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SExpandableArea)
				.Visibility_Lambda([this] { return NumSelectedActors > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
				.InitiallyCollapsed(true)
				.BorderImage(FAppStyle::GetNoBrush())
				.BodyBorderImage(FAppStyle::GetNoBrush())
				.HeaderPadding(FMargin(0.f, 2.f))
				.Padding(FMargin(16.f, 2.f, 0.f, 0.f))
				.AreaTitleFont(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.AreaTitle_Lambda([this]
				{
					return FText::Format(LOCTEXT("SelectedActorsTitle", "Selected actors ({0})"), NumSelectedActors);
				})
				.BodyContent()
				[
					SNew(SBox)
					.MaxDesiredHeight(160.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							VMatUI::HintText(TAttribute<FText>::CreateLambda([this] { return SelectionLabels; }))
						]
					]
				]
			]
		];
}

FVMatPreset& SVMatPaletteTab::GetCurrentPreset()
{
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	if (FVMatPreset* Preset = Settings.FindPreset(CurrentPresetName))
	{
		return *Preset;
	}

	// 이름으로 못 찾으면 첫번째거 써. preset은 항상 하나 이상 있어
	CurrentPresetName = Settings.Presets[0].Name;
	return Settings.Presets[0];
}

void SVMatPaletteTab::SelectPreset(const FString& Name)
{
	CurrentPresetName = Name;

	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	Settings.LastPresetName = Name;
	Settings.Save();

	RebuildMaterialGrid();
}

void SVMatPaletteTab::SyncPresetFromSettings()
{
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	if (Settings.LastPresetName != CurrentPresetName && Settings.FindPreset(Settings.LastPresetName))
	{
		CurrentPresetName = Settings.LastPresetName;
		RebuildMaterialGrid();
	}
}

void SVMatPaletteTab::RefreshPresetOptions()
{
	PresetNameOptions.Reset();
	for (const FVMatPreset& Preset : UVMatPresetSettings::Get().Presets)
	{
		PresetNameOptions.Add(MakeShared<FString>(Preset.Name));
	}

	if (PresetCombo.IsValid())
	{
		PresetCombo->RefreshOptions();
	}
}

FText SVMatPaletteTab::GetCurrentPresetText() const
{
	return FText::FromString(CurrentPresetName);
}

void SVMatPaletteTab::OnPresetSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	if (Item.IsValid() && *Item != CurrentPresetName)
	{
		SelectPreset(*Item);
	}
}

FReply SVMatPaletteTab::OnRenamePresetClicked()
{
	bIsRenaming = true;
	PresetNameBox->SetText(GetCurrentPresetText());
	PresetNameSwitcher->SetActiveWidgetIndex(1);
	return FReply::Handled().SetUserFocus(PresetNameBox.ToSharedRef(), EFocusCause::SetDirectly);
}

void SVMatPaletteTab::OnPresetNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	// Enter 치고 나서 포커스 빠질때 한번 더 들어와서 막아둬
	if (!bIsRenaming)
	{
		return;
	}
	bIsRenaming = false;
	PresetNameSwitcher->SetActiveWidgetIndex(0);

	// Esc면 취소
	if (CommitType == ETextCommit::OnCleared)
	{
		return;
	}

	const FString NewName = NewText.ToString().TrimStartAndEnd();
	if (NewName.IsEmpty() || NewName == CurrentPresetName)
	{
		return;
	}

	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	FVMatPreset& Preset = GetCurrentPreset();
	Preset.Name = Settings.MakeUniqueName(NewName);

	CurrentPresetName = Preset.Name;
	Settings.LastPresetName = Preset.Name;
	Settings.Save();
	RefreshPresetOptions();
}

FReply SVMatPaletteTab::OnNewPresetClicked()
{
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	const FString Name = Settings.MakeUniqueName(TEXT("New Preset"));
	Settings.Presets.AddDefaulted_GetRef().Name = Name;

	RefreshPresetOptions();
	SelectPreset(Name);
	return FReply::Handled();
}

FReply SVMatPaletteTab::OnDuplicatePresetClicked()
{
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	FVMatPreset Copy = GetCurrentPreset();
	Copy.Name = Settings.MakeUniqueName(Copy.Name + TEXT(" Copy"));
	Settings.Presets.Add(Copy);

	RefreshPresetOptions();
	SelectPreset(Copy.Name);
	return FReply::Handled();
}

FReply SVMatPaletteTab::OnDeletePresetClicked()
{
	const FText Message = FText::Format(LOCTEXT("DeletePresetConfirm", "Delete preset \"{0}\"?"), FText::FromString(CurrentPresetName));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	Settings.Presets.RemoveAll([this](const FVMatPreset& Preset) { return Preset.Name == CurrentPresetName; });

	// preset이 하나도 없으면 안돼서 빈거 하나 다시 만들어
	if (Settings.Presets.IsEmpty())
	{
		Settings.Presets.AddDefaulted_GetRef().Name = TEXT("Default");
	}

	RefreshPresetOptions();
	SelectPreset(Settings.Presets[0].Name);
	return FReply::Handled();
}

void SVMatPaletteTab::RebuildMaterialGrid()
{
	MaterialGrid->ClearChildren();

	for (const TSoftObjectPtr<UMaterialInterface>& SoftMaterial : GetCurrentPreset().Materials)
	{
		// preset에는 있는데 프로젝트에서 못 찾는 material은 지우지 말고 Missing으로 보여줘
		UMaterialInterface* Material = SoftMaterial.LoadSynchronous();
		TSharedPtr<FAssetThumbnail> Thumbnail;
		if (Material)
		{
			Thumbnail = MakeShared<FAssetThumbnail>(Material, VMatPalette::ThumbnailSize, VMatPalette::ThumbnailSize, ThumbnailPool);
		}

		MaterialGrid->AddSlot()
		[
			SNew(SVMatMaterialTile)
			.Thumbnail(Thumbnail)
			.ThumbnailSize(VMatPalette::ThumbnailSize)
			.Name(FText::FromString(Material ? Material->GetName() : SoftMaterial.GetAssetName()))
			.Path(FText::FromString(SoftMaterial.ToString()))
			.ApplyHint(LOCTEXT("TileApplyHint", "Click to apply to the selected meshes."))
			.OnApply(this, &SVMatPaletteTab::ApplyMaterial, SoftMaterial)
			.OnRemove(this, &SVMatPaletteTab::RemoveMaterial, SoftMaterial)
			.OnBrowse(this, &SVMatPaletteTab::BrowseToMaterial, SoftMaterial)
		];
	}
}

bool SVMatPaletteTab::HasMaterials() const
{
	const FVMatPreset* Preset = UVMatPresetSettings::Get().FindPreset(CurrentPresetName);
	return Preset && !Preset->Materials.IsEmpty();
}

void SVMatPaletteTab::AddMaterials(TConstArrayView<FAssetData> Assets)
{
	// material이랑 material instance만 넣어. 이미 있는건 또 안 넣어
	FVMatPreset& Preset = GetCurrentPreset();
	int32 NumAdded = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (!Asset.IsInstanceOf(UMaterialInterface::StaticClass()))
		{
			continue;
		}

		const TSoftObjectPtr<UMaterialInterface> SoftMaterial(Asset.GetSoftObjectPath());
		if (!Preset.Materials.Contains(SoftMaterial))
		{
			Preset.Materials.Add(SoftMaterial);
			++NumAdded;
		}
	}

	if (NumAdded == 0)
	{
		FNotificationInfo Info(LOCTEXT("NothingToAdd", "No new materials to add. Select materials in the Content Browser first."));
		Info.ExpireDuration = 3.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	UVMatPresetSettings::Get().Save();
	RebuildMaterialGrid();
}

bool SVMatPaletteTab::AreAssetsAcceptable(TArrayView<FAssetData> Assets) const
{
	return Assets.ContainsByPredicate([](const FAssetData& Asset) { return Asset.IsInstanceOf(UMaterialInterface::StaticClass()); });
}

void SVMatPaletteTab::OnAssetsDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets)
{
	AddMaterials(Assets);
}

FReply SVMatPaletteTab::OnAddSelectedClicked()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowser.Get().GetSelectedAssets(SelectedAssets);

	AddMaterials(SelectedAssets);
	return FReply::Handled();
}

void SVMatPaletteTab::ApplyMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial)
{
	VMatApplier::ApplyMaterial(SoftMaterial.LoadSynchronous());
}

void SVMatPaletteTab::RemoveMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial)
{
	GetCurrentPreset().Materials.Remove(SoftMaterial);
	UVMatPresetSettings::Get().Save();
	RebuildMaterialGrid();
}

void SVMatPaletteTab::BrowseToMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial)
{
	if (UMaterialInterface* Material = SoftMaterial.LoadSynchronous())
	{
		GEditor->SyncBrowserToObjects(TArray<UObject*>{ Material });
	}
}

void SVMatPaletteTab::RefreshSelectionSummary()
{
	const FVMatApplyTargets Targets = VMatApplier::CollectFromSelection();

	NumSelectedActors = Targets.NumSelectedActors;
	NumMeshesToChange = Targets.Meshes.Num();
	SkippedText = VMatApplier::DescribeSkipped(Targets);
	SelectionLabels = FText::FromString(FString::Join(Targets.ActorLabels, TEXT("\n")));

	SkippedMultiSlotMeshes.Reset();
	for (UStaticMesh* Mesh : Targets.MultiSlotMeshes)
	{
		SkippedMultiSlotMeshes.Add(Mesh);
	}
}

void SVMatPaletteTab::OnSelectionChanged(UObject* Selection)
{
	RefreshSelectionSummary();
}

void SVMatPaletteTab::OnShowMultiSlotClicked()
{
	TArray<UStaticMesh*> Meshes;
	for (const TWeakObjectPtr<UStaticMesh>& Mesh : SkippedMultiSlotMeshes)
	{
		if (Mesh.IsValid())
		{
			Meshes.Add(Mesh.Get());
		}
	}
	OnShowMultiSlotMeshes.ExecuteIfBound(Meshes);
}

#undef LOCTEXT_NAMESPACE
