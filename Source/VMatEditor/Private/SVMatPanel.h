#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SVMatMultiSlotTab;
class SVMatPaletteTab;
class SWidgetSwitcher;
class UStaticMesh;

// V-Mat 메인 창. 위에 탭 버튼, 아래에 Palette / Multi-slot 탭
class SVMatPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVMatPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	enum class EPage : uint8
	{
		Palette,
		MultiSlot
	};

	void ShowPage(EPage Page);
	void ShowMultiSlotMeshes(const TArray<UStaticMesh*>& Meshes);
	FText GetMultiSlotTabText() const;

	EPage CurrentPage = EPage::Palette;
	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	TSharedPtr<SVMatPaletteTab> PaletteTab;
	TSharedPtr<SVMatMultiSlotTab> MultiSlotTab;
};
