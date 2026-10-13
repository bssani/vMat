#include "SVMatPanel.h"

#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "SVMatMultiSlotTab.h"
#include "SVMatPaletteTab.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SVMatPanel"

void SVMatPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(FMargin(8.f, 8.f, 8.f, 4.f))
			[
				SNew(SSegmentedControl<EPage>)
				.Value_Lambda([this] { return CurrentPage; })
				.OnValueChanged(this, &SVMatPanel::ShowPage)
				+ SSegmentedControl<EPage>::Slot(EPage::Palette)
				.Icon(FSlateIconFinder::FindIconBrushForClass(UMaterial::StaticClass()))
				.Text(LOCTEXT("PaletteTab", "Palette"))
				.ToolTip(LOCTEXT("PaletteTabTooltip", "Apply preset materials to the selected meshes"))
				+ SSegmentedControl<EPage>::Slot(EPage::MultiSlot)
				.Icon(FSlateIconFinder::FindIconBrushForClass(UStaticMesh::StaticClass()))
				.Text(this, &SVMatPanel::GetMultiSlotTabText)
				.ToolTip(LOCTEXT("MultiSlotTabTooltip", "Find meshes with more than one material slot and set materials per slot"))
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(PageSwitcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(PaletteTab, SVMatPaletteTab)
				.OnShowMultiSlotMeshes(this, &SVMatPanel::ShowMultiSlotMeshes)
			]
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(MultiSlotTab, SVMatMultiSlotTab)
			]
		]
	];
}

void SVMatPanel::ShowPage(EPage Page)
{
	CurrentPage = Page;
	PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(Page));

	// 두 탭이 preset을 같이 써서, 넘어갈때 다른 탭에서 바꾼걸 반영해
	if (Page == EPage::MultiSlot)
	{
		// 창 열때마다 훑지 말고 Multi-slot 탭 처음 볼때만 훑어
		MultiSlotTab->ScanIfNeeded();
		MultiSlotTab->RefreshMaterialStrip();
	}
	else
	{
		PaletteTab->SyncPresetFromSettings();
	}
}

void SVMatPanel::ShowMultiSlotMeshes(const TArray<UStaticMesh*>& Meshes)
{
	CurrentPage = EPage::MultiSlot;
	PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(EPage::MultiSlot));
	MultiSlotTab->RefreshMaterialStrip();
	MultiSlotTab->RevealMeshes(Meshes);
}

FText SVMatPanel::GetMultiSlotTabText() const
{
	const TOptional<int32> Count = MultiSlotTab.IsValid() ? MultiSlotTab->GetFoundCount() : TOptional<int32>();
	if (!Count.IsSet())
	{
		return LOCTEXT("MultiSlotTab", "Multi-slot");
	}
	return FText::Format(LOCTEXT("MultiSlotTabCount", "Multi-slot ({0})"), Count.GetValue());
}

#undef LOCTEXT_NAMESPACE
