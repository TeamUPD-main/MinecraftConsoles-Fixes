#include "stdafx.h"

#include "UI.h"
#include "UIScene_AchievementsMenu.h"

#if !defined(_XBOX) && !defined(_DURANGO)

#include "StatsCounter.h"

#include "..\..\..\Minecraft.World\Achievements.h"
#include "..\..\..\Minecraft.World\Achievement.h"
//#include "..\..\..\Minecraft.World\ItemInstance.h"
#include "..\..\..\Minecraft.World\net.minecraft.locale.h"

UIScene_AchievementsMenu::UIScene_AchievementsMenu(int iPad, void *initData, UILayer *parentLayer)
	: UIScene(iPad, parentLayer),
	  m_statsCounter(nullptr),
	  m_selectedAchievementIndex(0),
	//  m_selectedIconIsGolden(false),
	  m_bIgnoreInput(true)
{
	// Setup the Iggy references we need for this scene
	initialiseMovie();
	m_funcSetAchievementDescription = registerFastName(L"SetAchievementDescription");

	m_bIgnoreInput = false;

	Minecraft *minecraft = Minecraft::GetInstance();
	if (minecraft != nullptr && iPad >= 0 && iPad < XUSER_MAX_COUNT)
	{
		m_statsCounter = minecraft->stats[iPad];
	}

	m_labelAchievementsTitle.init((UIString)IDS_ACHIEVEMENTS);
	m_labelSelectedName.init(L"");
	m_labelSelectedDescription.init(L"");
//	m_labelSelectedState.init(L"");

	populateAchievementsList();
	updateSelectedAchievement(0);
}

UIScene_AchievementsMenu::~UIScene_AchievementsMenu()
{
}

void UIScene_AchievementsMenu::updateTooltips()
{
	ui.SetTooltips(m_iPad, -1, IDS_TOOLTIPS_BACK);
}

void UIScene_AchievementsMenu::updateComponents()
{
	m_parentLayer->showComponent(m_iPad, eUIComponent_Panorama, !app.GetGameStarted());
	m_parentLayer->showComponent(m_iPad, eUIComponent_Logo, false);
}

wstring UIScene_AchievementsMenu::getMoviePath()
{
/*#if defined(_WINDOWS64)
	// Windows build fallback: reuse existing LeaderboardMenu SWF to avoid requiring new assets.
	return L"LeaderboardMenu";
#else
	// Loads AchievementsMenu1080.swf / AchievementsMenu720.swf / AchievementsMenu480.swf / AchievementsMenuVita.swf*/
	return L"AchievementsMenu";
//#endif
}

void UIScene_AchievementsMenu::handleReload()
{
	navigateBack();
}

void UIScene_AchievementsMenu::populateAchievementsList()
{
	m_achievements.clear();

	if (Achievements::achievements != nullptr)
	{
		m_achievements.reserve(Achievements::achievements->size());
		for (Achievement *ach : *Achievements::achievements)
		{
			if (ach != nullptr)
			{
				m_achievements.push_back(ach);
			}
		}
	}

	// List SWF is expected to expose LeaderboardList-style methods:
	// InitLeaderboard / AddDataSet / ResetLeaderboard / SetupTitles / SetColumnIcon (compatible).
	const int totalEntries = static_cast<int>(m_achievements.size());

	m_listAchievements.clearList();
/*	m_listAchievements.setupTitles(L"", app.GetString(IDS_ACHIEVEMENTS));
	m_listAchievements.initLeaderboard(0, totalEntries, 1);

#if defined(_WINDOWS64)
	// Ensure the reused LeaderboardMenu SWF has a visible icon column and triggers custom draw.
	// We draw the selected achievement icon in customDraw() anyway.
	m_listAchievements.setColumnIcon(0, UIControl_LeaderboardList::e_ICON_TYPE_CLIMBED);
#endif*/

	for (int i = 0; i < totalEntries; i++)
	{
		Achievement *ach = m_achievements[i];
	//	const bool isLast = i == (totalEntries - 1);

		bool hasTaken = false;
		bool canTake = true;
		if (m_statsCounter != nullptr)
		{
			hasTaken = m_statsCounter->hasTaken(ach);
			canTake = m_statsCounter->canTake(ach);
		}

/*		wstring stateText;
		if (hasTaken)
		{
			stateText = I18n::get(L"achievement.taken");
		}
		else if (canTake)
		{
			stateText = I18n::get(L"achievement.get");
		}
		else
		{
			if (ach->reqs != nullptr)
			{
				stateText = I18n::get(L"achievement.requires", ach->reqs->name);
			}
			else
			{
				stateText = I18n::get(L"achievement.requires", L"");
			}
		}*/
		m_listAchievements.addNewItem(i, getAchievementTextureName(ach));
		m_listAchievements.enableItem(i, hasTaken || canTake);
	}

		// iId: use list index so the Iggy selection callback can directly map back to m_achievements[index].
	/*	m_listAchievements.addDataSet(
			isLast,
			i,              // iId
			i + 1,          // iRank
			ach->name,      // gamertag column
			false,          // bDisplayMessage
			stateText,      // col0
			L"", L"", L"", L"", L"", L"");*/
	if (totalEntries > 0)
	{
		m_listAchievements.highlightItem(0);
	}
}

void UIScene_AchievementsMenu::updateSelectedAchievement(int index)
{
	if (index < 0 || index >= static_cast<int>(m_achievements.size()))
		return;

	m_selectedAchievementIndex = index;

	Achievement *ach = m_achievements[index];
	if (ach == nullptr)
		return;

/*	m_selectedIconItem = ach->icon;
	m_selectedIconIsGolden = ach->isGolden();*/

	bool hasTaken = false;
	bool canTake = true;
	if (m_statsCounter != nullptr)
	{
		hasTaken = m_statsCounter->hasTaken(ach);
		canTake = m_statsCounter->canTake(ach);
	}

//	wstring stateText;
	wstring descriptionText;

	if (hasTaken)
	{
	//	stateText = I18n::get(L"achievement.taken");
		descriptionText = ach->getDescription();
	}
	else if (canTake)
	{
	//	stateText = I18n::get(L"achievement.get");
		descriptionText = ach->getDescription();
	}
	else
	{
		if (ach->reqs != nullptr)
		{
			descriptionText = I18n::get(L"achievement.requires", ach->reqs->name);
		}
		else
		{
			descriptionText = I18n::get(L"achievement.requires", L"");
		}
		descriptionText = stateText;
	}

	m_labelSelectedName.setLabel(UIString(ach->name), true);
//	m_labelSelectedState.setLabel(UIString(stateText), true);
//	m_labelSelectedDescription.setLabel(UIString(descriptionText), true);
	setAchievementDescription(descriptionText);
}

void UIScene_AchievementsMenu::setAchievementDescription(const wstring &description)
{
	m_labelSelectedDescription.setLabel(UIString(description), true);

	IggyDataValue result;
	IggyDataValue value[1];

	IggyStringUTF16 stringVal;
	stringVal.string = (IggyUTF16 *)description.c_str();
	stringVal.length = description.length();
	value[0].type = IGGY_DATATYPE_string_UTF16;
	value[0].string16 = stringVal;

	IggyPlayerCallMethodRS(getMovie(), &result, IggyPlayerRootPath(getMovie()), m_funcSetAchievementDescription, 1, value);
}

wstring UIScene_AchievementsMenu::getAchievementTextureName(const Achievement *achievement) const
{
	if (achievement == Achievements::openInventory) return L"achievement.openInventory";
	if (achievement == Achievements::mineWood) return L"achievement.mineWood";
	if (achievement == Achievements::buildWorkbench) return L"achievement.buildWorkBench";
	if (achievement == Achievements::buildPickaxe) return L"achievement.buildPickaxe";
	if (achievement == Achievements::buildFurnace) return L"achievement.buildFurnace";
	if (achievement == Achievements::acquireIron) return L"achievement.acquireIron";
	if (achievement == Achievements::buildHoe) return L"achievement.buildHoe";
	if (achievement == Achievements::makeBread) return L"achievement.makeBread";
	if (achievement == Achievements::bakeCake) return L"achievement.bakeCake";
	if (achievement == Achievements::buildBetterPickaxe) return L"achievement.buildBetterPickaxe";
	if (achievement == Achievements::cookFish) return L"achievement.cookFish";
	if (achievement == Achievements::onARail) return L"achievement.onARail";
	if (achievement == Achievements::buildSword) return L"achievement.buildSword";
	if (achievement == Achievements::killEnemy) return L"achievement.killEnemy";
	if (achievement == Achievements::killCow) return L"achievement.killCow";
	if (achievement == Achievements::flyPig) return L"achievement.flyPig";
	if (achievement == Achievements::leaderOfThePack) return L"achievement.leaderOfThePack";
	if (achievement == Achievements::MOARTools) return L"achievement.MOARTools";
	if (achievement == Achievements::dispenseWithThis) return L"achievement.dispenseWithThis";
	if (achievement == Achievements::InToTheNether) return L"achievement.InToTheNether";
	if (achievement == Achievements::mine100Blocks) return L"achievement.mine100Blocks";
	if (achievement == Achievements::kill10Creepers) return L"achievement.kill10Creepers";
	if (achievement == Achievements::eatPorkChop) return L"achievement.eatPorkChop";
	if (achievement == Achievements::play100Days) return L"achievement.play100Days";
	if (achievement == Achievements::arrowKillCreeper) return L"achievement.arrowKillCreeper";
	if (achievement == Achievements::socialPost) return L"achievement.socialPost";
	if (achievement == Achievements::snipeSkeleton) return L"achievement.snipeSkeleton";
	if (achievement == Achievements::diamonds) return L"achievement.diamonds";
	if (achievement == Achievements::ghast) return L"achievement.ghast";
	if (achievement == Achievements::blazeRod) return L"achievement.blazeRod";
	if (achievement == Achievements::potion) return L"achievement.potion";
	if (achievement == Achievements::theEnd) return L"achievement.theEnd";
	if (achievement == Achievements::winGame) return L"achievement.theEnd2";
	if (achievement == Achievements::enchantments) return L"achievement.enchantments";

	return achievement != nullptr ? achievement->name : L"";
}

void UIScene_AchievementsMenu::handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled)
{
	if (m_bIgnoreInput && key != ACTION_MENU_CANCEL)
		return;

	ui.AnimateKeyPress(m_iPad, key, repeat, pressed, released);

	// If this isn't a press, do not action.
	if (!pressed)
		return;

	switch (key)
	{
	case ACTION_MENU_CANCEL:
		navigateBack();
		handled = true;
		break;
	default:
		// Let the SWF/list handle focus movement and selection.
		sendInputToMovie(key, repeat, pressed, released);
		break;
	}
}

void UIScene_AchievementsMenu::handleSelectionChanged(F64 selectedId)
{
	ui.PlayUISFX(eSFX_Focus);

	const int index = static_cast<int>(selectedId);
	updateSelectedAchievement(index);
}

void UIScene_AchievementsMenu::handleRequestMoreData(F64 startIndex, bool up)
{
	// Current implementation loads the full achievement list up-front.
	// If the SWF uses virtualization and requests partial ranges, this can be extended later.
	(void)startIndex;
	(void)up;
}

/*void UIScene_AchievementsMenu::customDraw(IggyCustomDrawCallbackRegion *region)
{
	int slotId = -1;

	if (region == nullptr || region->name == nullptr)
		return;

	swscanf(static_cast<wchar_t *>(region->name), L"slot_%d", &slotId);

	if (slotId == 0 && m_selectedIconItem != nullptr)
	{
		// The SWF movie is expected to expose a custom-draw slot named `slot_0`
		// for the selected achievement icon.
		customDrawSlotControl(region, m_iPad, m_selectedIconItem, 1.0f, m_selectedIconIsGolden, true);
	}
}*/

#endif // !defined(_XBOX) && !defined(_DURANGO)

