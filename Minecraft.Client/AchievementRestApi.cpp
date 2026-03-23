#include "stdafx.h"
#include "AchievementRestApi.h"

#ifdef _WINDOWS64
extern wchar_t g_Win64BaseUrlW[256];
#endif

// Helper function to convert wide string to UTF-8
static std::string WideStringToUTF8(const std::wstring& wideStr)
{
	if (wideStr.empty()) return "";
	
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], (int)wideStr.size(), NULL, 0, NULL, NULL);
	std::string utf8Str(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wideStr[0], (int)wideStr.size(), &utf8Str[0], sizeNeeded, NULL, NULL);
	
	return utf8Str;
}

static std::wstring GetConfiguredBaseUrl()
{
#ifdef _WINDOWS64
	if (g_Win64BaseUrlW[0] != 0)
	{
		return g_Win64BaseUrlW;
	}
#endif
	return L"https://legacy-leaderboards.onrender.com";
}

static std::wstring BuildUrl(const std::wstring& baseUrl, const std::wstring& endpoint)
{
	std::wstring url = baseUrl;
	while (!url.empty() && url.back() == L'/')
	{
		url.pop_back();
	}

	if (endpoint.empty())
	{
		return url;
	}

	if (endpoint.front() != L'/')
	{
		url += L'/';
	}

	url += endpoint;
	return url;
}

static bool ParseHttpBaseAndPath(const std::wstring& fullUrl, std::wstring& host, INTERNET_PORT& port, std::wstring& path, DWORD& requestFlags)
{
	size_t start = 0;
	bool https = false;

	if (fullUrl.rfind(L"https://", 0) == 0)
	{
		https = true;
		start = 8;
	}
	else if (fullUrl.rfind(L"http://", 0) == 0)
	{
		https = false;
		start = 7;
	}
	else
	{
		return false;
	}

	size_t slashPos = fullUrl.find(L'/', start);
	std::wstring authority = (slashPos == std::wstring::npos) ? fullUrl.substr(start) : fullUrl.substr(start, slashPos - start);
	path = (slashPos == std::wstring::npos) ? L"/" : fullUrl.substr(slashPos);

	if (authority.empty())
	{
		return false;
	}

	size_t colonPos = authority.rfind(L':');
	if (colonPos != std::wstring::npos)
	{
		host = authority.substr(0, colonPos);
		std::wstring portStr = authority.substr(colonPos + 1);
		int parsedPort = _wtoi(portStr.c_str());
		if (host.empty() || parsedPort <= 0 || parsedPort > 65535)
		{
			return false;
		}
		port = static_cast<INTERNET_PORT>(parsedPort);
	}
	else
	{
		host = authority;
		port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
	}

	requestFlags = https ? WINHTTP_FLAG_SECURE : 0;
	return true;
}

bool AchievementRestApi::RegisterPlayer(const std::wstring& playerUid, const std::wstring& playerName)
{
	return PostToAchievementApi(playerUid, playerName);
}

bool AchievementRestApi::PlayerExists(const std::wstring& playerUid)
{
	return GetPlayerExists(playerUid);
}

bool AchievementRestApi::GetPlayerExists(const std::wstring& playerUid)
{
	// API endpoint: <base_url>/api/player?uid=<playerUid>
	// GET request to check if player exists

	HINTERNET hSession = nullptr;
	HINTERNET hConnect = nullptr;
	HINTERNET hRequest = nullptr;
	std::wstring host;
	std::wstring requestPath;
	INTERNET_PORT port = 0;
	DWORD requestFlags = 0;

	try
	{
		// Create WinHTTP session
		hSession = WinHttpOpen(
			L"MinecraftConsole/1.0",
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0
		);

		if (!hSession)
		{
			return false;
		}

		// Set timeout to 5 seconds
		WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 5000);

		std::wstring fullUrl = BuildUrl(GetConfiguredBaseUrl(), L"/api/player?uid=" + playerUid);
		if (!ParseHttpBaseAndPath(fullUrl, host, port, requestPath, requestFlags))
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Create connection to configured host/port
		hConnect = WinHttpConnect(
			hSession,
			host.c_str(),
			port,
			0
		);

		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Create HTTP GET request
		hRequest = WinHttpOpenRequest(
			hConnect,
			L"GET",
			requestPath.c_str(),
			L"HTTP/1.1",
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			requestFlags
		);

		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Send the GET request
		if (!WinHttpSendRequest(
			hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0,
			NULL,
			0,
			0,
			0
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Wait for response
		if (!WinHttpReceiveResponse(hRequest, nullptr))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Get status code
		DWORD dwStatusCode = 0;
		DWORD dwSize = sizeof(dwStatusCode);

		if (!WinHttpQueryHeaders(
			hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&dwStatusCode,
			&dwSize,
			WINHTTP_NO_HEADER_INDEX
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Player exists if we get 200 OK
		bool bExists = (dwStatusCode == 200);

		// Clean up
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		return bExists;
	}
	catch (...)
	{
		if (hRequest) WinHttpCloseHandle(hRequest);
		if (hConnect) WinHttpCloseHandle(hConnect);
		if (hSession) WinHttpCloseHandle(hSession);
		return false;
	}
}

bool AchievementRestApi::PostToAchievementApi(const std::wstring& uid, const std::wstring& name)
{
	// API endpoint: <base_url>/api/player/add
	// Parameters: uid, name
	
	HINTERNET hSession = nullptr;
	HINTERNET hConnect = nullptr;
	HINTERNET hRequest = nullptr;
	std::wstring host;
	std::wstring requestPath;
	INTERNET_PORT port = 0;
	DWORD requestFlags = 0;
	
	try
	{
		// Create WinHTTP session
		hSession = WinHttpOpen(
			L"MinecraftConsole/1.0",
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0
		);
		
		if (!hSession)
		{
			return false;
		}
		
		// Set timeout to 5 seconds
		WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 5000);
		
		std::wstring fullUrl = BuildUrl(GetConfiguredBaseUrl(), L"/api/player/add");
		if (!ParseHttpBaseAndPath(fullUrl, host, port, requestPath, requestFlags))
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Create connection to configured host/port
		hConnect = WinHttpConnect(
			hSession,
			host.c_str(),
			port,
			0
		);
		
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return false;
		}
		
		// Create HTTP request
		hRequest = WinHttpOpenRequest(
			hConnect,
			L"POST",
			requestPath.c_str(),
			L"HTTP/1.1",
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			requestFlags
		);
		
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}
		
		// Prepare POST data: uid=<uid>&name=<name>
		std::wstring postDataWide = L"uid=" + uid + L"&name=" + name;
		std::string postDataUTF8 = WideStringToUTF8(postDataWide);
		
		// Add Content-Type header
		if (!WinHttpAddRequestHeaders(
			hRequest,
			L"Content-Type: application/x-www-form-urlencoded",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}
		
		// Send the POST request
		if (!WinHttpSendRequest(
			hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0,
			(LPVOID)postDataUTF8.c_str(),
			(DWORD)postDataUTF8.length(),
			(DWORD)postDataUTF8.length(),
			0
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}
		
		// Wait for response
		if (!WinHttpReceiveResponse(hRequest, nullptr))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}
		
		// Get status code
		DWORD dwStatusCode = 0;
		DWORD dwSize = sizeof(dwStatusCode);
		
		if (!WinHttpQueryHeaders(
			hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&dwStatusCode,
			&dwSize,
			WINHTTP_NO_HEADER_INDEX
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}
		
		// Consider 200-299 as successful
		bool bSuccess = (dwStatusCode >= 200 && dwStatusCode < 300);
		
		// Clean up
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		
		return bSuccess;
	}
	catch (...)
	{
		if (hRequest) WinHttpCloseHandle(hRequest);
		if (hConnect) WinHttpCloseHandle(hConnect);
		if (hSession) WinHttpCloseHandle(hSession);
		return false;
	}
}

bool AchievementRestApi::UnlockAchievement(const std::wstring& playerUid, int achievementId)
{
	return PostUnlockAchievementApi(playerUid, achievementId);
}

bool AchievementRestApi::PostUnlockAchievementApi(const std::wstring& playerUid, int achievementId)
{
	// API endpoint: <base_url>/api/achievement/add
	// Parameters: player_uid, achievement_id

	HINTERNET hSession = nullptr;
	HINTERNET hConnect = nullptr;
	HINTERNET hRequest = nullptr;
	std::wstring host;
	std::wstring requestPath;
	INTERNET_PORT port = 0;
	DWORD requestFlags = 0;

	try
	{
		// Create WinHTTP session
		hSession = WinHttpOpen(
			L"MinecraftConsole/1.0",
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0
		);

		if (!hSession)
		{
			return false;
		}

		// Set timeout to 5 seconds
		WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 5000);

		std::wstring fullUrl = BuildUrl(GetConfiguredBaseUrl(), L"/api/achievement/add");
		if (!ParseHttpBaseAndPath(fullUrl, host, port, requestPath, requestFlags))
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Create connection to configured host/port
		hConnect = WinHttpConnect(
			hSession,
			host.c_str(),
			port,
			0
		);

		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Create HTTP request
		hRequest = WinHttpOpenRequest(
			hConnect,
			L"POST",
			requestPath.c_str(),
			L"HTTP/1.1",
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			requestFlags
		);

		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Prepare POST data: player_uid=<playerUid>&achievement_id=<achievementId>
		std::wstring postDataWide = L"player_uid=" + playerUid + L"&achievement_id=" + std::to_wstring(achievementId);
		std::string postDataUTF8 = WideStringToUTF8(postDataWide);

		// Add Content-Type header
		if (!WinHttpAddRequestHeaders(
			hRequest,
			L"Content-Type: application/x-www-form-urlencoded",
			(ULONG)-1L,
			WINHTTP_ADDREQ_FLAG_ADD
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Send the POST request
		if (!WinHttpSendRequest(
			hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0,
			(LPVOID)postDataUTF8.c_str(),
			(DWORD)postDataUTF8.length(),
			(DWORD)postDataUTF8.length(),
			0
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Wait for response
		if (!WinHttpReceiveResponse(hRequest, nullptr))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Get status code
		DWORD dwStatusCode = 0;
		DWORD dwSize = sizeof(dwStatusCode);

		if (!WinHttpQueryHeaders(
			hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&dwStatusCode,
			&dwSize,
			WINHTTP_NO_HEADER_INDEX
		))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		// Consider 200-299 as successful
		bool bSuccess = (dwStatusCode >= 200 && dwStatusCode < 300);

		// Clean up
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		return bSuccess;
	}
	catch (...)
	{
		if (hRequest) WinHttpCloseHandle(hRequest);
		if (hConnect) WinHttpCloseHandle(hConnect);
		if (hSession) WinHttpCloseHandle(hSession);
		return false;
	}
}
