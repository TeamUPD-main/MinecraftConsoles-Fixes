#include "stdafx.h"
#include "UI.h"
#include "UIControl_AchievementList.h"

UIControl_AchievementList::UIControl_AchievementList()
{
}

bool UIControl_AchievementList::setupControl(UIScene *scene, IggyValuePath *parent, const string &controlName)
{
	UIControl::setControlType(UIControl::eButtonList);
	bool success = UIControl_Base::setupControl(scene, parent, controlName);

	m_funcAddNewItem = registerFastName(L"addNewItem");
	m_funcEnableItem = registerFastName(L"EnableItem");
	m_funcRemoveAllItems = registerFastName(L"removeAllItems");
	m_funcHighlightItem = registerFastName(L"HighlightItem");

	return success;
}

void UIControl_AchievementList::clearList()
{
	IggyDataValue result;
	IggyPlayerCallMethodRS(m_parentScene->getMovie(), &result, getIggyValuePath(), m_funcRemoveAllItems, 0, nullptr);
}

void UIControl_AchievementList::addNewItem(int id, const wstring &textureName)
{
	IggyDataValue result;
	IggyDataValue value[2];

	value[0].type = IGGY_DATATYPE_number;
	value[0].number = id;

	IggyStringUTF16 stringVal;
	stringVal.string = (IggyUTF16 *)textureName.c_str();
	stringVal.length = textureName.length();
	value[1].type = IGGY_DATATYPE_string_UTF16;
	value[1].string16 = stringVal;

	IggyPlayerCallMethodRS(m_parentScene->getMovie(), &result, getIggyValuePath(), m_funcAddNewItem, 2, value);
}

void UIControl_AchievementList::enableItem(int id, bool enabled)
{
	IggyDataValue result;
	IggyDataValue value[2];

	value[0].type = IGGY_DATATYPE_number;
	value[0].number = id;

	value[1].type = IGGY_DATATYPE_boolean;
	value[1].boolval = enabled;

	IggyPlayerCallMethodRS(m_parentScene->getMovie(), &result, getIggyValuePath(), m_funcEnableItem, 2, value);
}

void UIControl_AchievementList::highlightItem(int id)
{
	IggyDataValue result;
	IggyDataValue value[1];

	value[0].type = IGGY_DATATYPE_number;
	value[0].number = id;

	IggyPlayerCallMethodRS(m_parentScene->getMovie(), &result, getIggyValuePath(), m_funcHighlightItem, 1, value);
}
