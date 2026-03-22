#pragma once

#include "stdafx.h"
#include "Common\Leaderboards\LeaderboardManager.h"

#include <string>
#include <vector>

class WindowsLeaderboardManager : public LeaderboardManager
{
public:
	WindowsLeaderboardManager();
	virtual ~WindowsLeaderboardManager();

	virtual void Tick() {}

	//Open a session
	virtual bool OpenSession() { return true; }

	//Close a session
	virtual void CloseSession() {}

	//Delete a session
	virtual void DeleteSession() {}

	//Write the given stats
	//This is called synchronously and will not free any memory allocated for views when it is done

	virtual bool WriteStats(unsigned int viewCount, ViewIn views);

	virtual bool ReadStats_Friends(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int startIndex, unsigned int readCount);
	virtual bool ReadStats_MyScore(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int readCount);
	virtual bool ReadStats_TopRank(LeaderboardReadListener *callback, int difficulty, EStatsType type, unsigned int startIndex, unsigned int readCount);

	//Perform a flush of the stats
	virtual void FlushStats() {}

	//Cancel the current operation
	virtual void CancelOperation() {}

	//Is the leaderboard manager idle.
	virtual bool isIdle() { return true; }

private:
	// Stable backing storage for ViewOut returned to the UI.
	std::vector<ReadScore> m_lastReadScores;
	ReadView m_lastReadView;

	std::wstring m_baseUrl;
};
