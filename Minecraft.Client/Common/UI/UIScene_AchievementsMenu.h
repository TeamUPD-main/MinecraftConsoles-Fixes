#pragma once

#include "UIScene.h"

#if !defined(_XBOX) && !defined(_DURANGO)

class Achievement;
class StatsCounter;

// Iggy-based achievements UI (SWF movie: AchievementsMenu*.swf).
class UIScene_AchievementsMenu : public UIScene
{
private:
	StatsCounter *m_statsCounter;
	IggyName m_funcSetAchievementDescription;

	vector<Achievement *> m_achievements;
	int m_selectedAchievementIndex;

	UIControl m_controlAchievementsListContainer;
	UIControl_AchievementList m_listAchievements;
	UIControl_Label m_labelAchievementsTitle;
	UIControl_Label m_labelSelectedName;
	UIControl_HTMLLabel m_labelSelectedDescription;

	bool m_bIgnoreInput;

	UI_BEGIN_MAP_ELEMENTS_AND_NAMES(UIScene)
		UI_MAP_ELEMENT(m_controlAchievementsListContainer, "AchievementsListContainer")
		UI_BEGIN_MAP_CHILD_ELEMENTS(m_controlAchievementsListContainer)
			UI_MAP_ELEMENT(m_listAchievements, "AchievementsList")
		UI_END_MAP_CHILD_ELEMENTS()

		UI_MAP_ELEMENT(m_labelAchievementsTitle, "AcheivementsLabel")
		UI_MAP_ELEMENT(m_labelSelectedName, "AchievementName")
		UI_MAP_ELEMENT(m_labelSelectedDescription, "AchievementDescription")
	UI_END_MAP_ELEMENTS_AND_NAMES()

	void populateAchievementsList();
	void updateSelectedAchievement(int index);
	void setAchievementDescription(const wstring &description);
	wstring getAchievementTextureName(const Achievement *achievement) const;

public:
	UIScene_AchievementsMenu(int iPad, void *initData, UILayer *parentLayer);
	~UIScene_AchievementsMenu();

	virtual void updateTooltips();
	virtual void updateComponents();

	virtual EUIScene getSceneType() { return eUIScene_AchievementsMenu; }

protected:
	virtual wstring getMoviePath();

public:
	virtual void handleReload();

	// INPUT
	virtual void handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled);

	// Iggy callbacks
	virtual void handleSelectionChanged(F64 selectedId);
	virtual void handleRequestMoreData(F64 startIndex, bool up);
};

#endif // !defined(_XBOX) && !defined(_DURANGO)

