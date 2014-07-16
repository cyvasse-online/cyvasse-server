/* Copyright 2014 Jonas Platte
 *
 * This file is part of Cyvasse Online.
 *
 * Cyvasse Online is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * Cyvasse Online is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "match_manager.hpp"

#include <tntdb/connect.h>
#include <tntdb/error.h>
#include <tntdb/statement.h>
#include "match.hpp"
#include "db_config.hpp"

using namespace cyvmath;

namespace cyvdb
{
	MatchManager::MatchManager()
		: _conn(tntdb::connectCached(DBConfig::glob().getMatchDataUrl()))
	{ }

	Match MatchManager::getMatch(const std::string& matchID)
	{
		try
		{
			tntdb::Row row =
				_conn.prepare(
					"SELECT rule_set, searching_for_player FROM matches "
					"WHERE match_id = :id"
				)
				.set("id", matchID)
				.selectRow();

			return Match(matchID, StrToRuleSet(row.getString(0)), row.getBool(1));
		}
		catch(tntdb::NotFound&)
		{
			return Match();
		}
	}

	void MatchManager::addMatch(Match& match)
	{
		// TODO
	}
}
