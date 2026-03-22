#pragma once
#include <winsock2.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

class AchievementRestApi
{
public:
	// Make a GET request to check if a player exists
	// Returns true if player exists, false otherwise
	static bool PlayerExists(const std::wstring& playerUid);

	// Make a POST request to register player with the achievement backend
	// Returns true if successful, false otherwise
	static bool RegisterPlayer(const std::wstring& playerUid, const std::wstring& playerName);

	// Make a POST request to unlock an achievement
	// Returns true if successful, false otherwise
	static bool UnlockAchievement(const std::wstring& playerUid, int achievementId);

private:
	static bool GetPlayerExists(const std::wstring& playerUid);
	static bool PostToAchievementApi(const std::wstring& uid, const std::wstring& name);
	static bool PostUnlockAchievementApi(const std::wstring& playerUid, int achievementId);
};
