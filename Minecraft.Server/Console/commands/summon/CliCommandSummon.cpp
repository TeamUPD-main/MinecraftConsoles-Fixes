#include "stdafx.h"

#include "CliCommandSummon.h"

#include "..\..\ServerCliEngine.h"
#include "..\..\ServerCliParser.h"
#include "..\CommandParsing.h"
#include "..\..\..\..\Minecraft.World\GameCommandPacket.h"
#include "..\..\..\..\Minecraft.World\SummonCommand.h"

namespace ServerRuntime
{
	namespace
	{
		constexpr const char *kSummonUsage = "summon <entityType|entityId> <x> <y> <z>";
	}

	const char *CliCommandSummon::Name() const
	{
		return "summon";
	}

	const char *CliCommandSummon::Usage() const
	{
		return kSummonUsage;
	}

	const char *CliCommandSummon::Description() const
	{
		return "Summon an entity via Minecraft.World command dispatcher.";
	}

	bool CliCommandSummon::Execute(const ServerCliParsedLine &line, ServerCliEngine *engine)
	{
		if (line.tokens.size() != 5)
		{
			engine->LogWarn(std::string("Usage: ") + kSummonUsage);
			return false;
		}

		const std::string &entityToken = line.tokens[1];

		int x = 0;
		int y = 0;
		int z = 0;

		if (!CommandParsing::TryParseInt(line.tokens[2], &x))
		{
			engine->LogWarn("Invalid x coordinate: " + line.tokens[2]);
			return false;
		}
		if (!CommandParsing::TryParseInt(line.tokens[3], &y))
		{
			engine->LogWarn("Invalid y coordinate: " + line.tokens[3]);
			return false;
		}
		if (!CommandParsing::TryParseInt(line.tokens[4], &z))
		{
			engine->LogWarn("Invalid z coordinate: " + line.tokens[4]);
			return false;
		}

		std::wstring entityType(entityToken.begin(), entityToken.end());

		std::shared_ptr<GameCommandPacket> packet = SummonCommand::preparePacket(entityType, x, y, z);
		if (packet == nullptr)
		{
			engine->LogError("Failed to build summon command packet.");
			return false;
		}

		return engine->DispatchWorldCommand(packet->command, packet->data);
	}
}