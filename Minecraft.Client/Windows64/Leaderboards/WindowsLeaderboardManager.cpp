#include "stdafx.h"

#include "WindowsLeaderboardManager.h"

#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

// Reuse the header-only json dependency already vendored in this repo (used by Minecraft.Server).
#include "..\..\..\Minecraft.Server\vendor\nlohmann\json.hpp"

#pragma comment(lib, "winhttp.lib")

namespace
{
	using OrderedJson = nlohmann::ordered_json;

	static std::wstring GetEnvW(const wchar_t *key)
	{
		if (!key) return L"";

		wchar_t buffer[2048];
		const DWORD bufferCount = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
		DWORD len = GetEnvironmentVariableW(key, buffer, bufferCount);
		if (len == 0 || len >= bufferCount)
		{
			return L"";
		}
		return std::wstring(buffer, buffer + len);
	}

	static std::wstring DefaultBaseUrl()
	{
		// Local-only default; can be overridden via env var.
		return L"https://legacy-leaderboards.onrender.com/";
	}

	static std::string GetDefaultUserId()
	{
		// Let devs pin a specific user id without needing sign-in wiring.
		std::wstring w = GetEnvW(L"MC_LB_USER_ID");
		if (!w.empty()) return std::string(w.begin(), w.end());
		return "0";
	}

	static std::string UidToString(PlayerUID uid)
	{
		// Windows64 uses a numeric PlayerUID; your REST API expects a numeric user_id.
		if (uid == 0) return GetDefaultUserId();
		return std::to_string(static_cast<unsigned long long>(uid));
	}

	static std::wstring ToW(const std::string &s)
	{
		if (s.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
		if (len <= 0) return L"";
		std::wstring out;
		out.resize(static_cast<size_t>(len));
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
		return out;
	}

	static std::string Narrow(const std::wstring &w)
	{
		if (w.empty()) return "";
		int bytes = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
		if (bytes <= 0) return "";
		std::string out;
		out.resize(static_cast<size_t>(bytes));
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), bytes, nullptr, nullptr);
		return out;
	}

	static std::string Narrow(const wchar_t *w)
	{
		return w ? Narrow(std::wstring(w)) : "";
	}

	struct HttpResult
	{
		DWORD status = 0;
		std::string body;
	};

	// Very small WinHTTP wrapper for local REST calls.
	static HttpResult HttpRequest(const std::wstring &baseUrl, const std::wstring &method, const std::wstring &pathAndQuery, const std::string &body = "", const std::wstring &contentType = L"application/json")
	{
		HttpResult out;

		std::wstring fullUrl = baseUrl;
		if (!fullUrl.empty() && fullUrl.back() == L'/' && !pathAndQuery.empty() && pathAndQuery.front() == L'/')
		{
			fullUrl.pop_back();
		}
		fullUrl += pathAndQuery;

		URL_COMPONENTS components{};
		components.dwStructSize = sizeof(components);
		components.dwSchemeLength = static_cast<DWORD>(-1);
		components.dwHostNameLength = static_cast<DWORD>(-1);
		components.dwUrlPathLength = static_cast<DWORD>(-1);
		components.dwExtraInfoLength = static_cast<DWORD>(-1);

		if (!WinHttpCrackUrl(fullUrl.c_str(), 0, 0, &components))
		{
			return out;
		}

		std::wstring host(components.lpszHostName, components.dwHostNameLength);
		std::wstring urlPath(components.lpszUrlPath, components.dwUrlPathLength);
		std::wstring extraInfo(components.lpszExtraInfo, components.dwExtraInfoLength);
		std::wstring objectPath = urlPath + extraInfo;

		HINTERNET hSession = WinHttpOpen(L"MinecraftConsolesLeaderboards/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession) return out;

		HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), components.nPort, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return out;
		}

		DWORD flags = 0;
		if (components.nScheme == INTERNET_SCHEME_HTTPS)
		{
			flags |= WINHTTP_FLAG_SECURE;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), objectPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return out;
		}

		std::wstring headers;
		if (!body.empty())
		{
			headers = L"Content-Type: " + contentType + L"\r\n";
		}

		BOOL sent = WinHttpSendRequest(
			hRequest,
			headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
			headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
			body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
			body.empty() ? 0 : static_cast<DWORD>(body.size()),
			body.empty() ? 0 : static_cast<DWORD>(body.size()),
			0);

		if (sent && WinHttpReceiveResponse(hRequest, nullptr))
		{
			DWORD statusSize = sizeof(out.status);
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &out.status, &statusSize, WINHTTP_NO_HEADER_INDEX);

			DWORD available = 0;
			do
			{
				available = 0;
				if (!WinHttpQueryDataAvailable(hRequest, &available)) break;
				if (available == 0) break;

				std::string chunk;
				chunk.resize(available);
				DWORD read = 0;
				if (!WinHttpReadData(hRequest, chunk.data(), available, &read)) break;
				chunk.resize(read);
				out.body += chunk;
			} while (available > 0);
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		return out;
	}

	static const wchar_t *DifficultyToString(int difficulty)
	{
		// UI uses 0..3 (Peaceful/Easy/Normal/Hard). Kills leaderboard skips Peaceful in the UI,
		// but we still map it for completeness.
		switch (difficulty)
		{
		case 0: return L"peaceful";
		case 1: return L"easy";
		case 2: return L"normal";
		case 3: return L"hard";
		default: return L"normal";
		}
	}

	static const wchar_t *TypeToString(LeaderboardManager::EStatsType type)
	{
		switch (type)
		{
		case LeaderboardManager::eStatsType_Travelling: return L"travelling";
		case LeaderboardManager::eStatsType_Mining: return L"mining";
		case LeaderboardManager::eStatsType_Farming: return L"farming";
		case LeaderboardManager::eStatsType_Kills: return L"kills";
		default: return L"travelling";
		}
	}

	static bool ParseEntries(const std::string &jsonText, std::vector<LeaderboardManager::ReadScore> *outScores, int *outMaxRank)
	{
		if (!outScores) return false;
		outScores->clear();
		if (outMaxRank) *outMaxRank = 0;

		if (jsonText.empty())
		{
			return false;
		}

		// Parse in non-throwing mode. Some builds disable JSON exceptions,
		// and throwing parse failures would terminate the process.
		OrderedJson j = OrderedJson::parse(jsonText, nullptr, false);
		if (j.is_discarded())
		{
			return false;
		}

		if (!j.is_array())
		{
			// Allow single object responses too.
			if (j.is_object())
			{
				OrderedJson arr = OrderedJson::array();
				arr.push_back(j);
				j = std::move(arr);
			}
			else
			{
				return false;
			}
		}

		for (const auto &row : j)
		{
			if (!row.is_object()) continue;

			LeaderboardManager::ReadScore score{};
			score.m_uid = 0;
			score.m_rank = row.value("rank", 0);
			score.m_totalScore = row.value("total_score", 0);
			score.m_statsSize = 0;
			score.m_idsErrorMessage = 0;
			std::fill(std::begin(score.m_statsData), std::end(score.m_statsData), 0UL);

			if (row.contains("player") && row["player"].is_object())
			{
				const auto &p = row["player"];
				std::string uid = p.value("uid", "");
				std::string name = p.value("name", "");
				score.m_name = ToW(name);
				// We can't store string UIDs in PlayerUID, so hash it down to 64-bit for identity.
				if (!uid.empty())
				{
					std::hash<std::string> h;
					score.m_uid = static_cast<PlayerUID>(static_cast<unsigned long long>(h(uid)));
				}
			}

			if (row.contains("stats") && row["stats"].is_object())
			{
				const auto &stats = row["stats"];
				if (stats.contains("data") && stats["data"].is_object())
				{
					unsigned int idx = 0;
					for (auto it = stats["data"].begin(); it != stats["data"].end() && idx < LeaderboardManager::ReadScore::STATSDATA_MAX; ++it, ++idx)
					{
						if (it.value().is_number_integer())
						{
							score.m_statsData[idx] = it.value().get<unsigned long>();
						}
					}
					score.m_statsSize = static_cast<unsigned short>(idx);
				}
			}

			// Avoid std::max here: windows.h may define max as a macro.
			if (outMaxRank)
			{
				const int r = static_cast<int>(score.m_rank);
				*outMaxRank = (*outMaxRank > r) ? *outMaxRank : r;
			}
			outScores->push_back(std::move(score));
		}

		return true;
	}
}

LeaderboardManager *LeaderboardManager::m_instance = new WindowsLeaderboardManager(); //Singleton instance of the LeaderboardManager

WindowsLeaderboardManager::WindowsLeaderboardManager()
{
	m_lastReadView.m_numQueries = 0;
	m_lastReadView.m_queries = nullptr;

	std::wstring env = GetEnvW(L"MC_LB_API_BASE_URL");
	m_baseUrl = !env.empty() ? env : DefaultBaseUrl();
}

WindowsLeaderboardManager::~WindowsLeaderboardManager()
{
	m_lastReadScores.clear();
	m_lastReadView.m_numQueries = 0;
	m_lastReadView.m_queries = nullptr;
}

bool WindowsLeaderboardManager::ReadStats_TopRank(LeaderboardReadListener *callback, int difficulty, EStatsType type, unsigned int startIndex, unsigned int readCount)
{
	// UI uses 1-based "startIndex" (rank); API uses 0-based start offset.
	const unsigned int start = (startIndex > 0) ? (startIndex - 1) : 0;

	std::wstringstream qs;
	qs << L"/api/leaderboard/top/?difficulty=" << DifficultyToString(difficulty)
	   << L"&type=" << TypeToString(type)
	   << L"&start=" << start
	   << L"&count=" << readCount;

	HttpResult res = HttpRequest(m_baseUrl, L"GET", qs.str());

	LeaderboardManager::eStatsReturn ret = LeaderboardManager::eStatsReturn_NetworkError;
	int maxRank = 0;

	if (res.status >= 200 && res.status < 300)
	{
		if (ParseEntries(res.body, &m_lastReadScores, &maxRank) && !m_lastReadScores.empty())
		{
			ret = LeaderboardManager::eStatsReturn_Success;
		}
		else
		{
			ret = LeaderboardManager::eStatsReturn_NoResults;
		}
	}

	m_lastReadView.m_numQueries = static_cast<unsigned int>(m_lastReadScores.size());
	m_lastReadView.m_queries = m_lastReadScores.empty() ? nullptr : m_lastReadScores.data();

	if (callback)
	{
		// numResults is treated by the UI as total leaderboard rows for TopRank/MyScore.
		const int candidate = static_cast<int>(start + m_lastReadScores.size());
		const int totalRows = (maxRank > candidate) ? maxRank : candidate;
		callback->OnStatsReadComplete(ret, totalRows, m_lastReadView);
	}
	return true;
}

bool WindowsLeaderboardManager::ReadStats_Friends(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int /*startIndex*/, unsigned int /*readCount*/)
{
	const std::string userId = UidToString(myUID);

	std::wstringstream qs;
	qs << L"/api/leaderboard/friends/?difficulty=" << DifficultyToString(difficulty)
	   << L"&type=" << TypeToString(type)
	   << L"&user_id=" << ToW(userId);

	HttpResult res = HttpRequest(m_baseUrl, L"GET", qs.str());

	LeaderboardManager::eStatsReturn ret = LeaderboardManager::eStatsReturn_NetworkError;

	int maxRank = 0;
	if (res.status >= 200 && res.status < 300)
	{
		if (ParseEntries(res.body, &m_lastReadScores, &maxRank) && !m_lastReadScores.empty())
		{
			ret = LeaderboardManager::eStatsReturn_Success;
		}
		else
		{
			ret = LeaderboardManager::eStatsReturn_NoResults;
		}
	}

	m_lastReadView.m_numQueries = static_cast<unsigned int>(m_lastReadScores.size());
	m_lastReadView.m_queries = m_lastReadScores.empty() ? nullptr : m_lastReadScores.data();

	if (callback)
	{
		// For Friends, the UI uses m_numQueries as the "total count" anyway.
		callback->OnStatsReadComplete(ret, static_cast<int>(m_lastReadScores.size()), m_lastReadView);
	}
	return true;
}

bool WindowsLeaderboardManager::ReadStats_MyScore(LeaderboardReadListener *callback, int difficulty, EStatsType type, PlayerUID myUID, unsigned int readCount)
{
	const std::string userId = UidToString(myUID);

	std::wstringstream qs;
	qs << L"/api/leaderboard/my-score/?difficulty=" << DifficultyToString(difficulty)
	   << L"&type=" << TypeToString(type)
	   << L"&user_id=" << ToW(userId)
	   << L"&count=" << readCount;

	HttpResult res = HttpRequest(m_baseUrl, L"GET", qs.str());

	LeaderboardManager::eStatsReturn ret = LeaderboardManager::eStatsReturn_NetworkError;
	int maxRank = 0;

	if (res.status >= 200 && res.status < 300)
	{
		if (ParseEntries(res.body, &m_lastReadScores, &maxRank) && !m_lastReadScores.empty())
		{
			ret = LeaderboardManager::eStatsReturn_Success;
		}
		else
		{
			ret = LeaderboardManager::eStatsReturn_NoResults;
		}
	}

	m_lastReadView.m_numQueries = static_cast<unsigned int>(m_lastReadScores.size());
	m_lastReadView.m_queries = m_lastReadScores.empty() ? nullptr : m_lastReadScores.data();

	if (callback)
	{
		const int candidate = static_cast<int>(m_lastReadScores.size());
		const int totalRows = (maxRank > candidate) ? maxRank : candidate;
		callback->OnStatsReadComplete(ret, totalRows, m_lastReadView);
	}
	return true;
}

bool WindowsLeaderboardManager::WriteStats(unsigned int viewCount, ViewIn views)
{
	if (!views || viewCount == 0) return false;

	const RegisterScore &in = views[0];
	const std::string userId = UidToString(0);

	OrderedJson payload;
	payload["difficulty"] = Narrow(DifficultyToString(in.m_difficulty));
	payload["type"] = Narrow(TypeToString(in.m_commentData.m_statsType));
	payload["user_id"] = userId;
	payload["total_score"] = in.m_score;
	payload["stats"] = OrderedJson::object();
	payload["stats"]["type"] = static_cast<int>(in.m_commentData.m_statsType);

	OrderedJson data = OrderedJson::object();
	switch (in.m_commentData.m_statsType)
	{
	case eStatsType_Kills:
		data["zombie"] = in.m_commentData.m_kills.m_zombie;
		data["skeleton"] = in.m_commentData.m_kills.m_skeleton;
		data["creeper"] = in.m_commentData.m_kills.m_creeper;
		data["spider"] = in.m_commentData.m_kills.m_spider;
		data["spiderJockey"] = in.m_commentData.m_kills.m_spiderJockey;
		data["zombiePigman"] = in.m_commentData.m_kills.m_zombiePigman;
		data["slime"] = in.m_commentData.m_kills.m_slime;
		break;
	case eStatsType_Mining:
		data["dirt"] = in.m_commentData.m_mining.m_dirt;
		data["stone"] = in.m_commentData.m_mining.m_stone;
		data["sand"] = in.m_commentData.m_mining.m_sand;
		data["cobblestone"] = in.m_commentData.m_mining.m_cobblestone;
		data["gravel"] = in.m_commentData.m_mining.m_gravel;
		data["clay"] = in.m_commentData.m_mining.m_clay;
		data["obsidian"] = in.m_commentData.m_mining.m_obsidian;
		break;
	case eStatsType_Farming:
		data["egg"] = in.m_commentData.m_farming.m_eggs;
		data["wheat"] = in.m_commentData.m_farming.m_wheat;
		data["mushroom"] = in.m_commentData.m_farming.m_mushroom;
		data["sugarcane"] = in.m_commentData.m_farming.m_sugarcane;
		data["milk"] = in.m_commentData.m_farming.m_milk;
		data["pumpkin"] = in.m_commentData.m_farming.m_pumpkin;
		break;
	case eStatsType_Travelling:
	default:
		data["walked"] = in.m_commentData.m_travelling.m_walked;
		data["fallen"] = in.m_commentData.m_travelling.m_fallen;
		data["minecart"] = in.m_commentData.m_travelling.m_minecart;
		data["boat"] = in.m_commentData.m_travelling.m_boat;
		break;
	}

	payload["stats"]["data"] = std::move(data);

	std::string body = payload.dump();
	HttpResult res = HttpRequest(m_baseUrl, L"POST", L"/api/leaderboard/write/", body, L"application/json");

	return (res.status >= 200 && res.status < 300);
}