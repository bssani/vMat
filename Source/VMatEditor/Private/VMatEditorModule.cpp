#include "VMatEditorModule.h"

#include "Framework/Docking/TabManager.h"
#include "Materials/Material.h"
#include "SVMatPanel.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FVMatEditorModule"

const FName FVMatEditorModule::PanelTabName(TEXT("VMatPanel"));

namespace VMatEditor
{
	static FSlateIcon GetPanelIcon()
	{
		return FSlateIconFinder::FindIconForClass(UMaterial::StaticClass());
	}
}

void FVMatEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PanelTabName, FOnSpawnTab::CreateRaw(this, &FVMatEditorModule::SpawnPanelTab))
		.SetDisplayName(LOCTEXT("PanelTabTitle", "V-Mat"))
		.SetTooltipText(LOCTEXT("PanelTabTooltip", "Open the V-Mat material palette"))
		.SetIcon(VMatEditor::GetPanelIcon())
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FVMatEditorModule::RegisterMenus));
}

void FVMatEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PanelTabName);
}

void FVMatEditorModule::RegisterMenus()
{
	// Tools 메뉴랑 툴바에 V-Mat 여는 버튼 넣어. 예전 Python 툴바 버튼 대신이야
	FToolMenuOwnerScoped OwnerScoped(this);

	const FUIAction OpenPanelAction(FExecuteAction::CreateRaw(this, &FVMatEditorModule::OpenPanel));
	const FText Label = LOCTEXT("OpenPanelLabel", "V-Mat");
	const FText Tooltip = LOCTEXT("OpenPanelTooltip", "Open the V-Mat material palette");

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& MenuSection = ToolsMenu->FindOrAddSection("VMat", LOCTEXT("ToolsSection", "V-Mat"));
	MenuSection.AddMenuEntry("OpenVMatPanel", Label, Tooltip, VMatEditor::GetPanelIcon(), OpenPanelAction);

	// UE5에서는 "LevelEditor.LevelEditorToolBar"가 화면에 안 나와. 플러그인 버튼은 Play 버튼 오른쪽 User 툴바에 넣어야 보여
	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& ToolbarSection = Toolbar->FindOrAddSection("VMat");
	FToolMenuEntry& ToolbarEntry = ToolbarSection.AddEntry(FToolMenuEntry::InitToolBarButton("OpenVMatPanel", OpenPanelAction, Label, Tooltip, VMatEditor::GetPanelIcon()));
	// 아이콘만 있으면 뭔지 모르니까 옆에 V-Mat 글자도 보이게
	ToolbarEntry.StyleNameOverride = "CalloutToolbar";
}

void FVMatEditorModule::OpenPanel()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PanelTabName);
}

TSharedRef<SDockTab> FVMatEditorModule::SpawnPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVMatPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVMatEditorModule, VMatEditor)
