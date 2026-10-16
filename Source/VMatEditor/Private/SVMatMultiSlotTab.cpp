#include "SVMatMultiSlotTab.h"

#include "AssetThumbnail.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "SVMatMaterialTile.h"
#include "SVMatMeshPreview.h"
#include "VMatMaterialApplier.h"
#include "VMatPresetSettings.h"
#include "VMatUI.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SVMatMultiSlotTab"

namespace VMatMultiSlotColumn
{
	static const FName Mesh(TEXT("Mesh"));
	static const FName Slots(TEXT("Slots"));
	static const FName Actors(TEXT("Actors"));
	static const FName Folder(TEXT("Folder"));
}

namespace VMatMultiSlotTab
{
	// mesh 에디터를 한번에 너무 많이 열면 에디터가 멈춰서 막아둬
	constexpr int32 MaxEditorsToOpen = 10;

	constexpr int32 SlotSwatchSize = 32;
	constexpr int32 StripThumbnailSize = 56;
}

// mesh 목록 한 줄. 마우스 올리면 slot별로 지금 들어있는 material 보여줘
class SVMatMultiSlotRow : public SMultiColumnTableRow<TSharedPtr<FVMatMultiSlotMesh>>
{
public:
	SLATE_BEGIN_ARGS(SVMatMultiSlotRow) {}
		SLATE_ARGUMENT(TSharedPtr<FVMatMultiSlotMesh>, Item)
		SLATE_ATTRIBUTE(FText, HighlightText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
	{
		Item = InArgs._Item;
		HighlightText = InArgs._HighlightText;

		FSuperRowType::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 3.f)), OwnerTable);
		SetToolTipText(TAttribute<FText>::CreateSP(this, &SVMatMultiSlotRow::GetSlotsToolTip));
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == VMatMultiSlotColumn::Mesh)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f)
				[
					SNew(SImage)
					.Image(FSlateIconFinder::FindIconBrushForClass(UStaticMesh::StaticClass()))
					.DesiredSizeOverride(FVector2D(14.f, 14.f))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Name))
					.HighlightText(HighlightText)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				];
		}

		if (ColumnName == VMatMultiSlotColumn::Slots || ColumnName == VMatMultiSlotColumn::Actors)
		{
			const int32 Value = ColumnName == VMatMultiSlotColumn::Slots ? Item->NumSlots : Item->Actors.Num();
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::AsNumber(Value))
				];
		}

		return SNew(SBox)
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Folder))
				.HighlightText(HighlightText)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			];
	}

private:
	FText GetSlotsToolTip() const
	{
		const UStaticMesh* Mesh = Item->Mesh.Get();
		if (!Mesh)
		{
			return LOCTEXT("MeshUnloaded", "This mesh is no longer loaded. Scan the level again.");
		}

		FString Text = Mesh->GetPathName();
		const TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();
		for (int32 SlotIndex = 0; SlotIndex < Materials.Num(); ++SlotIndex)
		{
			const UMaterialInterface* Material = Materials[SlotIndex].MaterialInterface;
			Text += FString::Printf(TEXT("\n  [%d] %s : %s"), SlotIndex, *Materials[SlotIndex].MaterialSlotName.ToString(), Material ? *Material->GetName() : TEXT("None"));
		}
		Text += TEXT("\n\nDouble-click to select its actors in the level.");
		return FText::FromString(Text);
	}

	TSharedPtr<FVMatMultiSlotMesh> Item;
	TAttribute<FText> HighlightText;
};

void SVMatMultiSlotTab::Construct(const FArguments& InArgs)
{
	SortColumn = VMatMultiSlotColumn::Mesh;
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(SearchBox, SSearchBox)
						.HintText(LOCTEXT("SearchHint", "Search mesh name or folder"))
						.OnTextChanged(this, &SVMatMultiSlotTab::OnSearchTextChanged)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						VMatUI::TextButton("Icons.Refresh", LOCTEXT("ScanLevel", "Scan level"),
							LOCTEXT("ScanLevelTooltip", "Find every mesh in the open level that has more than one material slot.\nOnly loaded actors are checked."),
							FOnClicked::CreateLambda([this]
							{
								Scan();
								return FReply::Handled();
							}))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.f, 6.f, 0.f, 0.f)
				[
					VMatUI::HintText(TAttribute<FText>::CreateSP(this, &SVMatMultiSlotTab::GetStatusText))
				]
			]
		]
		// 왼쪽은 mesh 목록이랑 slot 목록, 오른쪽은 미리보기. 경계 끌어서 크기 조절할 수 있어
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.6f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					MakeMeshSection()
				]
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					MakeSlotSection()
				]
			]
			+ SSplitter::Slot()
			.Value(0.4f)
			[
				MakePreviewSection()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeMaterialStrip()
		]
	];

	RefreshMaterialStrip();
	MapChangedHandle = FEditorDelegates::MapChange.AddSP(this, &SVMatMultiSlotTab::OnMapChanged);
}

SVMatMultiSlotTab::~SVMatMultiSlotTab()
{
	FEditorDelegates::MapChange.Remove(MapChangedHandle);
}

TSharedRef<SWidget> SVMatMultiSlotTab::MakeMeshSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
			.Padding(0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					// 몇천줄이어도 화면에 보이는 줄만 만들어서 괜찮아
					SAssignNew(ListView, SListView<FItem>)
					.ListItemsSource(&VisibleItems)
					.SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow(this, &SVMatMultiSlotTab::OnGenerateRow)
					.OnSelectionChanged(this, &SVMatMultiSlotTab::OnMeshSelectionChanged)
					.OnMouseButtonDoubleClick(this, &SVMatMultiSlotTab::OnItemDoubleClicked)
					.OnContextMenuOpening(this, &SVMatMultiSlotTab::OnContextMenuOpening)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(VMatMultiSlotColumn::Mesh)
						.DefaultLabel(LOCTEXT("MeshColumn", "Mesh"))
						.FillWidth(0.45f)
						.SortMode(this, &SVMatMultiSlotTab::GetSortMode, VMatMultiSlotColumn::Mesh)
						.OnSort(this, &SVMatMultiSlotTab::OnSortModeChanged)
						+ SHeaderRow::Column(VMatMultiSlotColumn::Slots)
						.DefaultLabel(LOCTEXT("SlotsColumn", "Slots"))
						.DefaultTooltip(LOCTEXT("SlotsColumnTooltip", "Number of material slots on the mesh"))
						.FixedWidth(56.f)
						.HAlignHeader(HAlign_Center)
						.HAlignCell(HAlign_Center)
						.SortMode(this, &SVMatMultiSlotTab::GetSortMode, VMatMultiSlotColumn::Slots)
						.OnSort(this, &SVMatMultiSlotTab::OnSortModeChanged)
						+ SHeaderRow::Column(VMatMultiSlotColumn::Actors)
						.DefaultLabel(LOCTEXT("ActorsColumn", "Actors"))
						.DefaultTooltip(LOCTEXT("ActorsColumnTooltip", "Number of actors in the level that use the mesh"))
						.FixedWidth(60.f)
						.HAlignHeader(HAlign_Center)
						.HAlignCell(HAlign_Center)
						.SortMode(this, &SVMatMultiSlotTab::GetSortMode, VMatMultiSlotColumn::Actors)
						.OnSort(this, &SVMatMultiSlotTab::OnSortModeChanged)
						+ SHeaderRow::Column(VMatMultiSlotColumn::Folder)
						.DefaultLabel(LOCTEXT("FolderColumn", "Folder"))
						.FillWidth(0.55f)
						.SortMode(this, &SVMatMultiSlotTab::GetSortMode, VMatMultiSlotColumn::Folder)
						.OnSort(this, &SVMatMultiSlotTab::OnSortModeChanged)
					)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(16.f, 40.f, 16.f, 16.f)
				[
					SNew(STextBlock)
					.Visibility(this, &SVMatMultiSlotTab::GetEmptyVisibility)
					.Text(this, &SVMatMultiSlotTab::GetEmptyText)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Justification(ETextJustify::Center)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(10.f, 6.f, 8.f, 6.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					VMatUI::HintText(TAttribute<FText>::CreateSP(this, &SVMatMultiSlotTab::GetSelectionText))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 2.f, 0.f)
				[
					VMatUI::TextButton("Icons.SelectInViewport", LOCTEXT("SelectInLevel", "Select in level"),
						LOCTEXT("SelectInLevelTooltip", "Select every actor that uses the chosen meshes and move the camera to them"),
						FOnClicked::CreateLambda([this]
						{
							SelectActorsInLevel();
							return FReply::Handled();
						}),
						TAttribute<bool>::CreateSP(this, &SVMatMultiSlotTab::HasSelection))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					VMatUI::IconButton("Icons.BrowseContent", LOCTEXT("BrowseTooltip", "Show the mesh assets in the Content Browser"),
						FOnClicked::CreateLambda([this]
						{
							BrowseToMeshes();
							return FReply::Handled();
						}),
						TAttribute<bool>::CreateSP(this, &SVMatMultiSlotTab::HasSelection))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					VMatUI::IconButton("Icons.Edit",
						FText::Format(LOCTEXT("OpenTooltip", "Open in the Static Mesh Editor (up to {0} at once)"), VMatMultiSlotTab::MaxEditorsToOpen),
						FOnClicked::CreateLambda([this]
						{
							OpenMeshEditors();
							return FReply::Handled();
						}),
						TAttribute<bool>::CreateSP(this, &SVMatMultiSlotTab::CanOpenMeshEditors))
				]
			]
		];
}

TSharedRef<SWidget> SVMatMultiSlotTab::MakeSlotSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(10.f, 6.f, 8.f, 6.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SlotsTitle", "Slots"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f)
				[
					VMatUI::HintText(TAttribute<FText>::CreateSP(this, &SVMatMultiSlotTab::GetSlotHeaderText))
				]
				// mesh 여러개 골랐을때만 묶는 기준 고를 수 있어
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SSegmentedControl<EVMatSlotGrouping>)
					.Visibility_Lambda([this] { return NumSlotMeshes > 1 ? EVisibility::Visible : EVisibility::Collapsed; })
					.Value_Lambda([this] { return Grouping; })
					.OnValueChanged(this, &SVMatMultiSlotTab::OnGroupingChanged)
					+ SSegmentedControl<EVMatSlotGrouping>::Slot(EVMatSlotGrouping::Material)
					.Text(LOCTEXT("GroupByMaterial", "By material"))
					.ToolTip(LOCTEXT("GroupByMaterialTooltip", "Group slots that currently use the same material"))
					+ SSegmentedControl<EVMatSlotGrouping>::Slot(EVMatSlotGrouping::SlotName)
					.Text(LOCTEXT("GroupBySlotName", "By slot name"))
					.ToolTip(LOCTEXT("GroupBySlotNameTooltip", "Group slots with the same slot name.\nDatasmith CAD imports name slots after the CAD material, so the same part color shares a name."))
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
			.Padding(0.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(SlotListView, SListView<FSlotItem>)
					.ListItemsSource(&SlotGroups)
					.SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow(this, &SVMatMultiSlotTab::OnGenerateSlotRow)
					.OnSelectionChanged(this, &SVMatMultiSlotTab::OnSlotSelectionChanged)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(16.f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this] { return SlotGroups.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.Text(LOCTEXT("SlotsEmpty", "Select a mesh above to see its slots."))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Justification(ETextJustify::Center)
				]
			]
		];
}

TSharedRef<SWidget> SVMatMultiSlotTab::MakePreviewSection()
{
	// 미리보기 위에 얹는 글자. 배경이 밝을수도 있어서 그림자 깔아
	auto OverlayText = [](TAttribute<FText> Text, const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font)
			.ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		.Padding(0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				// mesh 없을땐 숨겨둬. 숨어있으면 안 그려서 GPU도 안 써
				SAssignNew(MeshPreview, SVMatMeshPreview)
				.Visibility_Lambda([this] { return HasPreviewMesh() ? EVisibility::Visible : EVisibility::Hidden; })
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(10.f, 8.f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this] { return HasPreviewMesh() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					OverlayText(TAttribute<FText>::CreateLambda([this]
						{
							return PreviewItem.IsValid() ? FText::FromString(PreviewItem->Name) : FText::GetEmpty();
						}),
						FCoreStyle::GetDefaultFontStyle("Bold", 9), FLinearColor::White)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					OverlayText(TAttribute<FText>::CreateSP(this, &SVMatMultiSlotTab::GetPreviewHintText),
						FCoreStyle::GetDefaultFontStyle("Regular", 8), FLinearColor(1.f, 1.f, 1.f, 0.7f))
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(16.f)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this] { return HasPreviewMesh() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				.Text(LOCTEXT("PreviewEmpty", "Select a mesh to preview it."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Justification(ETextJustify::Center)
			]
		];
}

TSharedRef<SWidget> SVMatMultiSlotTab::MakeMaterialStrip()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
		.Padding(FMargin(8.f, 6.f, 8.f, 8.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(2.f, 0.f, 0.f, 0.f)
				[
					VMatUI::HintText(TAttribute<FText>::CreateSP(this, &SVMatMultiSlotTab::GetStripHintText))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox)
					.MinDesiredWidth(140.f)
					[
						SAssignNew(StripPresetCombo, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&StripPresetOptions)
						.ToolTipText(LOCTEXT("StripPresetTooltip", "Preset to pick materials from. Same presets as the Palette tab."))
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(*Item));
						})
						.OnSelectionChanged(this, &SVMatMultiSlotTab::OnStripPresetSelected)
						[
							SNew(STextBlock)
							.Text_Lambda([this] { return FText::FromString(StripPresetName); })
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				+ SScrollBox::Slot()
				[
					SAssignNew(MaterialStrip, SHorizontalBox)
				]
			]
		];
}

void SVMatMultiSlotTab::ScanIfNeeded()
{
	if (!bHasScanned)
	{
		Scan();
	}
}

void SVMatMultiSlotTab::RevealMeshes(const TArray<UStaticMesh*>& Meshes)
{
	// 마지막으로 훑은 뒤에 actor가 추가됐을수도 있어서 다시 훑어. 몇 ms면 끝나
	Scan();

	SearchText = FText::GetEmpty();
	SearchBox->SetText(FText::GetEmpty());
	RebuildVisibleItems();

	SelectMeshItems(TSet<UStaticMesh*>(Meshes));
	if (ListView->GetNumItemsSelected() > 0)
	{
		ListView->RequestScrollIntoView(ListView->GetSelectedItems()[0]);
	}
}

TOptional<int32> SVMatMultiSlotTab::GetFoundCount() const
{
	return bHasScanned ? TOptional<int32>(ScanResult.Meshes.Num()) : TOptional<int32>();
}

void SVMatMultiSlotTab::Scan()
{
	// 다시 훑어도 고르고 있던 mesh는 그대로 골라져 있게
	const TSet<UStaticMesh*> PreviouslySelected(GetSelectedMeshes());

	ScanResult = VMatMultiSlot::ScanLevel();
	bHasScanned = true;
	bLevelChanged = false;
	RebuildVisibleItems();
	SelectMeshItems(PreviouslySelected);
}

void SVMatMultiSlotTab::RebuildVisibleItems()
{
	const FString Filter = SearchText.ToString().TrimStartAndEnd();

	VisibleItems.Reset();
	for (const FItem& Item : ScanResult.Meshes)
	{
		if (Filter.IsEmpty() || Item->Name.Contains(Filter) || Item->Folder.Contains(Filter))
		{
			VisibleItems.Add(Item);
		}
	}

	const bool bAscending = SortMode != EColumnSortMode::Descending;
	const FName Column = SortColumn;
	VisibleItems.Sort([bAscending, Column](const FItem& A, const FItem& B)
	{
		int32 Compare = 0;
		if (Column == VMatMultiSlotColumn::Slots)
		{
			Compare = A->NumSlots - B->NumSlots;
		}
		else if (Column == VMatMultiSlotColumn::Actors)
		{
			Compare = A->Actors.Num() - B->Actors.Num();
		}
		else if (Column == VMatMultiSlotColumn::Folder)
		{
			Compare = A->Folder.Compare(B->Folder, ESearchCase::IgnoreCase);
		}

		// 같으면 이름순
		if (Compare == 0)
		{
			Compare = A->Name.Compare(B->Name, ESearchCase::IgnoreCase);
		}
		return bAscending ? Compare < 0 : Compare > 0;
	});

	ListView->RequestListRefresh();
}

void SVMatMultiSlotTab::SelectMeshItems(const TSet<UStaticMesh*>& Meshes)
{
	const TArray<FItem> Matches = VisibleItems.FilterByPredicate([&Meshes](const FItem& Item)
	{
		return Meshes.Contains(Item->Mesh.Get());
	});

	// 선택 바뀔때마다 slot 목록 만들지 말고 다 바꾸고 한번만
	bUpdatingMeshSelection = true;
	ListView->ClearSelection();
	ListView->SetItemSelection(Matches, true);
	bUpdatingMeshSelection = false;

	RefreshSlotGroups();
}

void SVMatMultiSlotTab::OnMapChanged(uint32 MapChangeFlags)
{
	if ((MapChangeFlags & MapChangeEventFlags::NewMap) == 0)
	{
		return;
	}

	// 다른 맵 결과가 남아있으면 헷갈리니까 비워. 다시 훑는건 탭 열때나 Scan 누를때
	bLevelChanged = bLevelChanged || bHasScanned;
	bHasScanned = false;
	ScanResult = FVMatMultiSlotScanResult();
	RebuildVisibleItems();
	SelectMeshItems({});
}

TSharedRef<ITableRow> SVMatMultiSlotTab::OnGenerateRow(FItem Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SVMatMultiSlotRow, OwnerTable)
		.Item(Item)
		.HighlightText_Lambda([this] { return SearchText; });
}

void SVMatMultiSlotTab::OnMeshSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo)
{
	// 여러개 골라도 미리보기엔 마지막에 누른 줄을 띄워
	if (Item.IsValid() && ListView->IsItemSelected(Item))
	{
		PreviewItem = Item;
	}

	if (!bUpdatingMeshSelection)
	{
		RefreshSlotGroups();
	}
}

void SVMatMultiSlotTab::OnItemDoubleClicked(FItem Item)
{
	SelectActorsInLevel();
}

TSharedPtr<SWidget> SVMatMultiSlotTab::OnContextMenuOpening()
{
	if (!HasSelection())
	{
		return nullptr;
	}

	FMenuBuilder Menu(true, nullptr);
	Menu.AddMenuEntry(
		LOCTEXT("MenuSelect", "Select in level"),
		LOCTEXT("MenuSelectTooltip", "Select every actor that uses these meshes and move the camera to them"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.SelectInViewport"),
		FUIAction(FExecuteAction::CreateSP(this, &SVMatMultiSlotTab::SelectActorsInLevel)));
	Menu.AddMenuEntry(
		LOCTEXT("MenuBrowse", "Browse to asset"),
		LOCTEXT("MenuBrowseTooltip", "Show the mesh assets in the Content Browser"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.BrowseContent"),
		FUIAction(FExecuteAction::CreateSP(this, &SVMatMultiSlotTab::BrowseToMeshes)));
	Menu.AddMenuEntry(
		LOCTEXT("MenuOpen", "Open in Static Mesh Editor"),
		FText::Format(LOCTEXT("MenuOpenTooltip", "Open the mesh editor (up to {0} at once)"), VMatMultiSlotTab::MaxEditorsToOpen),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateSP(this, &SVMatMultiSlotTab::OpenMeshEditors), FCanExecuteAction::CreateSP(this, &SVMatMultiSlotTab::CanOpenMeshEditors)));
	return Menu.MakeWidget();
}

void SVMatMultiSlotTab::OnSearchTextChanged(const FText& Text)
{
	SearchText = Text;
	RebuildVisibleItems();
}

void SVMatMultiSlotTab::OnSortModeChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewSortMode)
{
	SortColumn = ColumnId;
	SortMode = NewSortMode;
	RebuildVisibleItems();
}

EColumnSortMode::Type SVMatMultiSlotTab::GetSortMode(FName ColumnId) const
{
	return ColumnId == SortColumn ? SortMode : EColumnSortMode::None;
}

void SVMatMultiSlotTab::SelectActorsInLevel()
{
	if (!GEditor)
	{
		return;
	}

	// 같은 actor가 multi-slot mesh를 여러개 쓰고 있을수도 있어서 한번씩만 넣어
	TArray<AActor*> Actors;
	TSet<AActor*> SeenActors;
	for (const FItem& Item : ListView->GetSelectedItems())
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : Item->Actors)
		{
			AActor* Actor = WeakActor.Get();
			bool bSeen = false;
			if (Actor)
			{
				SeenActors.Add(Actor, &bSeen);
			}
			if (Actor && !bSeen)
			{
				Actors.Add(Actor);
			}
		}
	}

	if (Actors.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SelectActorsTransaction", "V-Mat: Select multi-slot actors"));
	USelection* Selection = GEditor->GetSelectedActors();
	Selection->BeginBatchSelectOperation();
	GEditor->SelectNone(false, true, false);
	for (AActor* Actor : Actors)
	{
		GEditor->SelectActor(Actor, true, false, true);
	}
	Selection->EndBatchSelectOperation(false);
	GEditor->NoteSelectionChange();

	GEditor->MoveViewportCamerasToActor(Actors, false);
}

void SVMatMultiSlotTab::BrowseToMeshes()
{
	const TArray<UObject*> Meshes(GetSelectedMeshes());
	if (GEditor && !Meshes.IsEmpty())
	{
		GEditor->SyncBrowserToObjects(Meshes);
	}
}

void SVMatMultiSlotTab::OpenMeshEditors()
{
	const TArray<UObject*> Meshes(GetSelectedMeshes());
	if (GEditor && !Meshes.IsEmpty() && Meshes.Num() <= VMatMultiSlotTab::MaxEditorsToOpen)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(Meshes);
	}
}

bool SVMatMultiSlotTab::HasSelection() const
{
	return ListView->GetNumItemsSelected() > 0;
}

bool SVMatMultiSlotTab::CanOpenMeshEditors() const
{
	const int32 NumSelected = ListView->GetNumItemsSelected();
	return NumSelected > 0 && NumSelected <= VMatMultiSlotTab::MaxEditorsToOpen;
}

TArray<UStaticMesh*> SVMatMultiSlotTab::GetSelectedMeshes() const
{
	TArray<UStaticMesh*> Meshes;
	for (const FItem& Item : ListView->GetSelectedItems())
	{
		if (UStaticMesh* Mesh = Item->Mesh.Get())
		{
			Meshes.Add(Mesh);
		}
	}
	return Meshes;
}

FText SVMatMultiSlotTab::GetStatusText() const
{
	if (!bHasScanned)
	{
		return bLevelChanged
			? LOCTEXT("StatusLevelChanged", "The level changed. Scan again.")
			: LOCTEXT("StatusNotScanned", "Not scanned yet.");
	}

	FNumberFormattingOptions MsFormat;
	MsFormat.MaximumFractionalDigits = 1;

	const FText Found = FText::Format(
		LOCTEXT("StatusFound", "{0} multi-slot {0}|plural(one=mesh,other=meshes)  ·  checked {1} meshes on {2} actors in {3} ms"),
		ScanResult.Meshes.Num(), ScanResult.NumMeshesScanned, ScanResult.NumActorsScanned, FText::AsNumber(ScanResult.Seconds * 1000.0, &MsFormat));

	if (!SearchText.IsEmpty())
	{
		return FText::Format(LOCTEXT("StatusFiltered", "Showing {0} of {1}"), VisibleItems.Num(), Found);
	}
	return Found;
}

FText SVMatMultiSlotTab::GetSelectionText() const
{
	const int32 NumSelected = ListView->GetNumItemsSelected();
	if (NumSelected == 0)
	{
		return LOCTEXT("NoRowsSelected", "Pick a mesh to edit its slots. Double-click selects it in the level.");
	}
	return FText::Format(LOCTEXT("RowsSelected", "{0} {0}|plural(one=mesh,other=meshes) selected"), NumSelected);
}

FText SVMatMultiSlotTab::GetEmptyText() const
{
	if (!bHasScanned)
	{
		return bLevelChanged
			? LOCTEXT("EmptyLevelChanged", "The level changed.\nClick Scan level to look again.")
			: LOCTEXT("EmptyNotScanned", "Click Scan level to find meshes\nwith more than one material slot.");
	}
	if (ScanResult.Meshes.IsEmpty())
	{
		return LOCTEXT("EmptyNoneFound", "No multi-slot meshes in this level.");
	}
	return LOCTEXT("EmptyNoMatch", "No meshes match the search.");
}

EVisibility SVMatMultiSlotTab::GetEmptyVisibility() const
{
	return VisibleItems.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

void SVMatMultiSlotTab::RefreshSlotGroups()
{
	// 다시 만들어도 고르고 있던 slot은 그대로 골라져 있게. material 바꾼 뒤에도 이어서 작업하기 편해
	TSet<TPair<UStaticMesh*, int32>> SelectedTargets;
	for (const FSlotItem& Group : SlotListView->GetSelectedItems())
	{
		for (const FVMatSlotTarget& Target : Group->Targets)
		{
			SelectedTargets.Add(MakeTuple(Target.Mesh.Get(), Target.SlotIndex));
		}
	}

	const TArray<UStaticMesh*> Meshes = GetSelectedMeshes();
	NumSlotMeshes = Meshes.Num();
	SlotGroups = VMatMultiSlot::GroupSlots(Meshes, Grouping);

	TArray<FSlotItem> ToSelect;
	for (const FSlotItem& Group : SlotGroups)
	{
		const bool bWasSelected = Group->Targets.ContainsByPredicate([&SelectedTargets](const FVMatSlotTarget& Target)
		{
			return SelectedTargets.Contains(MakeTuple(Target.Mesh.Get(), Target.SlotIndex));
		});
		if (bWasSelected)
		{
			ToSelect.Add(Group);
		}
	}

	SlotListView->ClearSelection();
	SlotListView->RequestListRefresh();
	SlotListView->SetItemSelection(ToSelect, true);

	RefreshPreview();
}

TSharedRef<ITableRow> SVMatMultiSlotTab::OnGenerateSlotRow(FSlotItem Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	UMaterialInterface* Material = Item->Material.Get();

	// 지금 들어있는 material 썸네일. 섞여 있거나 비어있으면 글자로
	TSharedRef<SWidget> Swatch = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ContentBrowser.AssetTileItem.ThumbnailAreaBackground"))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			VMatUI::HintText(Item->bMixedMaterials ? LOCTEXT("SwatchMixed", "Mix") : LOCTEXT("SwatchNone", "-"))
		];
	if (Material)
	{
		Swatch = MakeShared<FAssetThumbnail>(Material, VMatMultiSlotTab::SlotSwatchSize, VMatMultiSlotTab::SlotSwatchSize, ThumbnailPool)->MakeThumbnailWidget();
	}

	const FText MaterialName = Material
		? FText::FromString(Material->GetName())
		: (Item->bMixedMaterials ? LOCTEXT("MixedMaterials", "Mixed materials") : LOCTEXT("NoMaterial", "No material"));

	FText Label = FText::FromString(Item->Label);
	FText Detail;
	if (Item->SlotIndex != INDEX_NONE)
	{
		Label = FText::Format(LOCTEXT("SingleSlotLabel", "[{0}]  {1}"), Item->SlotIndex, Label);
		Detail = MaterialName;
	}
	else if (Grouping == EVMatSlotGrouping::Material)
	{
		Detail = FText::Format(LOCTEXT("MaterialGroupDetail", "{0} {0}|plural(one=slot,other=slots) in {1} {1}|plural(one=mesh,other=meshes)"), Item->Targets.Num(), Item->NumMeshes);
	}
	else
	{
		Detail = FText::Format(LOCTEXT("NameGroupDetail", "{0}  ·  {1} {1}|plural(one=mesh,other=meshes)"), MaterialName, Item->NumMeshes);
	}

	return SNew(STableRow<FSlotItem>, OwnerTable)
		.Padding(FMargin(6.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(VMatMultiSlotTab::SlotSwatchSize)
				.HeightOverride(VMatMultiSlotTab::SlotSwatchSize)
				[
					Swatch
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(Label)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 1.f, 0.f, 0.f)
				[
					VMatUI::HintText(Detail)
				]
			]
		];
}

void SVMatMultiSlotTab::OnGroupingChanged(EVMatSlotGrouping NewGrouping)
{
	Grouping = NewGrouping;
	RefreshSlotGroups();
}

void SVMatMultiSlotTab::OnSlotSelectionChanged(FSlotItem Item, ESelectInfo::Type SelectInfo)
{
	RefreshPreview();
}

void SVMatMultiSlotTab::RefreshPreview()
{
	// 마지막에 누른 줄이 선택에서 빠졌으면 다른걸로. 다시 훑은 뒤에도 같은 mesh면 그대로 둬서 카메라 안 튀게
	if (!PreviewItem.IsValid() || !ListView->IsItemSelected(PreviewItem))
	{
		const TArray<FItem> Selected = ListView->GetSelectedItems();
		const UStaticMesh* ShownMesh = MeshPreview->GetMesh();
		if (const FItem* Same = Selected.FindByPredicate([ShownMesh](const FItem& Item) { return ShownMesh && Item->Mesh.Get() == ShownMesh; }))
		{
			PreviewItem = *Same;
		}
		else
		{
			PreviewItem = Selected.IsEmpty() ? FItem() : Selected[0];
		}
	}

	UStaticMesh* Mesh = PreviewItem.IsValid() ? PreviewItem->Mesh.Get() : nullptr;
	MeshPreview->SetMesh(Mesh);

	// slot 골랐으면 고른 slot만 원래 material로 두고 나머지는 어둡게. 어느 부분인지 바로 보여
	TSet<int32> DimmedSlots;
	if (Mesh && HasSlotSelection())
	{
		TSet<int32> SelectedSlots;
		for (const FSlotItem& Group : SlotListView->GetSelectedItems())
		{
			for (const FVMatSlotTarget& Target : Group->Targets)
			{
				if (Target.Mesh.Get() == Mesh)
				{
					SelectedSlots.Add(Target.SlotIndex);
				}
			}
		}

		for (int32 SlotIndex = 0; SlotIndex < Mesh->GetStaticMaterials().Num(); ++SlotIndex)
		{
			if (!SelectedSlots.Contains(SlotIndex))
			{
				DimmedSlots.Add(SlotIndex);
			}
		}
	}
	MeshPreview->SetDimmedSlots(DimmedSlots);
}

bool SVMatMultiSlotTab::HasPreviewMesh() const
{
	return MeshPreview.IsValid() && MeshPreview->GetMesh() != nullptr;
}

FText SVMatMultiSlotTab::GetPreviewHintText() const
{
	TArray<FText> Lines;

	const int32 NumSelected = ListView->GetNumItemsSelected();
	if (NumSelected > 1)
	{
		Lines.Add(FText::Format(LOCTEXT("PreviewOneOfMany", "Last clicked of {0} selected meshes"), NumSelected));
	}
	if (HasSlotSelection())
	{
		Lines.Add(LOCTEXT("PreviewDimmed", "Selected slots in color, the rest dimmed"));
	}
	Lines.Add(LOCTEXT("PreviewControls", "Drag to orbit  ·  scroll to zoom  ·  F to re-center"));

	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

bool SVMatMultiSlotTab::HasSlotSelection() const
{
	return SlotListView->GetNumItemsSelected() > 0;
}

FText SVMatMultiSlotTab::GetSlotHeaderText() const
{
	if (NumSlotMeshes == 0)
	{
		return FText::GetEmpty();
	}
	if (NumSlotMeshes == 1 && ListView->GetNumItemsSelected() > 0)
	{
		return FText::FromString(ListView->GetSelectedItems()[0]->Name);
	}
	return FText::Format(LOCTEXT("SlotHeaderMeshes", "{0} meshes"), NumSlotMeshes);
}

void SVMatMultiSlotTab::RefreshMaterialStrip()
{
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();

	StripPresetOptions.Reset();
	for (const FVMatPreset& Preset : Settings.Presets)
	{
		StripPresetOptions.Add(MakeShared<FString>(Preset.Name));
	}
	StripPresetCombo->RefreshOptions();

	// Palette 탭에서 마지막에 쓰던 preset을 같이 써
	StripPresetName = Settings.FindPreset(Settings.LastPresetName)
		? Settings.LastPresetName
		: (Settings.Presets.IsEmpty() ? FString() : Settings.Presets[0].Name);

	RebuildStripTiles();
}

void SVMatMultiSlotTab::RebuildStripTiles()
{
	MaterialStrip->ClearChildren();

	const FVMatPreset* Preset = UVMatPresetSettings::Get().FindPreset(StripPresetName);
	if (!Preset || Preset->Materials.IsEmpty())
	{
		MaterialStrip->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.f, 12.f)
		[
			VMatUI::HintText(LOCTEXT("StripEmpty", "This preset is empty. Add materials in the Palette tab."))
		];
		return;
	}

	for (const TSoftObjectPtr<UMaterialInterface>& SoftMaterial : Preset->Materials)
	{
		UMaterialInterface* Material = SoftMaterial.LoadSynchronous();
		TSharedPtr<FAssetThumbnail> Thumbnail;
		if (Material)
		{
			Thumbnail = MakeShared<FAssetThumbnail>(Material, VMatMultiSlotTab::StripThumbnailSize, VMatMultiSlotTab::StripThumbnailSize, ThumbnailPool);
		}

		MaterialStrip->AddSlot()
		.AutoWidth()
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			// slot 안 골랐으면 눌러봤자 들어갈데가 없어서 꺼둬
			SNew(SVMatMaterialTile)
			.IsEnabled(this, &SVMatMultiSlotTab::HasSlotSelection)
			.Thumbnail(Thumbnail)
			.ThumbnailSize(VMatMultiSlotTab::StripThumbnailSize)
			.Name(FText::FromString(Material ? Material->GetName() : SoftMaterial.GetAssetName()))
			.Path(FText::FromString(SoftMaterial.ToString()))
			.ApplyHint(LOCTEXT("StripApplyHint", "Click to put it in the selected slots."))
			.OnApply(this, &SVMatMultiSlotTab::ApplyToSelectedSlots, SoftMaterial)
			.OnBrowse(this, &SVMatMultiSlotTab::BrowseToMaterial, SoftMaterial)
		];
	}
}

void SVMatMultiSlotTab::OnStripPresetSelected(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid() || *Item == StripPresetName)
	{
		return;
	}

	// Palette 탭도 이 preset으로 따라가게 저장해
	StripPresetName = *Item;
	UVMatPresetSettings& Settings = UVMatPresetSettings::Get();
	Settings.LastPresetName = StripPresetName;
	Settings.Save();

	RebuildStripTiles();
}

void SVMatMultiSlotTab::ApplyToSelectedSlots(TSoftObjectPtr<UMaterialInterface> SoftMaterial)
{
	TArray<FVMatMeshSlot> Slots;
	for (const FSlotItem& Group : SlotListView->GetSelectedItems())
	{
		for (const FVMatSlotTarget& Target : Group->Targets)
		{
			if (UStaticMesh* Mesh = Target.Mesh.Get())
			{
				Slots.Add({ Mesh, Target.SlotIndex });
			}
		}
	}

	VMatApplier::ApplyMaterialToSlots(SoftMaterial.LoadSynchronous(), Slots);

	// 들어있는 material이 바뀌었으니 slot 목록 다시 만들어. 고른 slot은 그대로 남아
	RefreshSlotGroups();
}

void SVMatMultiSlotTab::BrowseToMaterial(TSoftObjectPtr<UMaterialInterface> SoftMaterial)
{
	if (UMaterialInterface* Material = SoftMaterial.LoadSynchronous())
	{
		GEditor->SyncBrowserToObjects(TArray<UObject*>{ Material });
	}
}

FText SVMatMultiSlotTab::GetStripHintText() const
{
	const TArray<FSlotItem> SelectedGroups = SlotListView->GetSelectedItems();
	if (SelectedGroups.IsEmpty())
	{
		return LOCTEXT("StripHintNoSlots", "Select slots above, then click a material.");
	}

	int32 NumSlots = 0;
	TSet<UStaticMesh*> Meshes;
	for (const FSlotItem& Group : SelectedGroups)
	{
		NumSlots += Group->Targets.Num();
		for (const FVMatSlotTarget& Target : Group->Targets)
		{
			Meshes.Add(Target.Mesh.Get());
		}
	}
	return FText::Format(LOCTEXT("StripHint", "Click a material to put it in {0} {0}|plural(one=slot,other=slots) on {1} {1}|plural(one=mesh,other=meshes)"), NumSlots, Meshes.Num());
}

#undef LOCTEXT_NAMESPACE
