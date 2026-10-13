#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

class FVMatEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void OpenPanel();
	TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args);

	static const FName PanelTabName;
};
