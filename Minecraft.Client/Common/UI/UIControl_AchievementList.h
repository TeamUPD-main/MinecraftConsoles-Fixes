#pragma once

#include "UIControl_Base.h"

class UIControl_AchievementList : public UIControl_Base
{
private:
	IggyName m_funcAddNewItem;
	IggyName m_funcEnableItem;
	IggyName m_funcRemoveAllItems;
	IggyName m_funcHighlightItem;

public:
	UIControl_AchievementList();

	virtual bool setupControl(UIScene *scene, IggyValuePath *parent, const string &controlName);

	void clearList();
	void addNewItem(int id, const wstring &textureName);
	void enableItem(int id, bool enabled);
	void highlightItem(int id);
};
