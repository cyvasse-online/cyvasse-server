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

#include "cyvasse_server.hpp"

#include <chrono>
#include <thread>
#include <json/value.h>
#include <json/writer.h>
#include <cyvdb/match_manager.hpp>
#include "client_data.hpp"
#include "match_data.hpp"
#include "worker.hpp"

using namespace std;
using namespace std::chrono;
using namespace websocketpp;

CyvasseServer::CyvasseServer()
{
	using placeholders::_1;
	using placeholders::_2;

	// Initialize Asio Transport
	m_wsServer.init_asio();

	// Register handler callback
	m_wsServer.set_message_handler(bind(&CyvasseServer::onMessage, this, _1, _2));
	m_wsServer.set_close_handler(bind(&CyvasseServer::onClose, this, _1));
	//m_wsServer.set_http_handler(bind(&CyvasseServer::onHttpRequest, this, _1));
}

CyvasseServer::~CyvasseServer()
{
	if(m_data.running)
		stop();
}

void CyvasseServer::run(uint16_t port, unsigned nWorkers)
{
	using placeholders::_1;
	using placeholders::_2;
	using placeholders::_3;

	// static cast to select the right overload,
	// bind() to bind the m_wsServer object to the functor
	auto send_func =
		bind(static_cast<void (WSServer::*) (connection_hdl, const string&, frame::opcode::value)>(&WSServer::send),
			&m_wsServer, _1, _2, _3);

	// start worker threads
	assert(nWorkers != 0);
	for(unsigned i = 0; i < nWorkers; i++)
		m_workers.emplace(new Worker(m_data, send_func));

	// Listen on the specified port
	m_wsServer.listen(port);

	// Start the server accept loop
	m_wsServer.start_accept();

	// Start the ASIO io_service run loop
	m_wsServer.run();
}

void CyvasseServer::stop()
{
	m_data.running = false;
	m_data.jobCond.notify_all();
}

void CyvasseServer::onMessage(connection_hdl hdl, WSServer::message_ptr msg)
{
	// Queue message up for sending by processing thread
	unique_lock<mutex> lock(m_data.jobMtx);

	m_data.jobQueue.emplace(hdl, msg);

	lock.unlock();
	m_data.jobCond.notify_one();
}

void CyvasseServer::onClose(connection_hdl hdl)
{
	unique_lock<mutex> matchDataLock(m_data.matchDataMtx, defer_lock);
	unique_lock<mutex> clientDataLock(m_data.clientDataMtx);

	auto it1 = m_data.clientData.find(hdl);
	if(it1 == m_data.clientData.end())
		return;

	shared_ptr<ClientData> clientData = it1->second;
	m_data.clientData.erase(it1);
	clientDataLock.unlock();

	auto& dataSets = clientData->getMatchData().getClientDataSets();

	auto color = clientData->getPlayer().getColor();

	for(auto it2 : dataSets)
		if(*it2 == *clientData)
			dataSets.erase(it2);
		else
		{
			Json::Value chatMsg;
			chatMsg["messageType"]      = "request";
			chatMsg["action"]           = "chat message";
			chatMsg["param"]["sender"]  = "Server";
			chatMsg["param"]["message"] = PlayersColorToPrettyStr(color) + " left.";

			m_wsServer.send(it2->getConnHdl(), Json::FastWriter().write(chatMsg), frame::opcode::text);
		}

	if(dataSets.empty())
	{
		// if this was the last / only player connected
		// to this match, remove the match completely
		auto matchID = clientData->getMatchData().getMatch().getID();

		matchDataLock.lock();
		auto it3 = m_data.matchData.find(matchID);
		if(it3 != m_data.matchData.end())
			m_data.matchData.erase(it3);

		matchDataLock.unlock();

		thread([matchID]() {
			this_thread::sleep_for(milliseconds(50));
			cyvdb::MatchManager().removeMatch(matchID);
		}).detach();
	}
}

void CyvasseServer::onHttpRequest(connection_hdl)
{
	// TODO: send 301 moved permanently -> domain:80
}
