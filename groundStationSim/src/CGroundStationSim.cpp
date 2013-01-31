/**
 * @brief 
 * @file CGroundStationSim.cpp
 *
 * This file is created at Almende B.V. It is open-source software and part of the Common
 * Hybrid Agent Platform (CHAP). A toolbox with a lot of open-source tools, ranging from
 * thread pools and TCP/IP components to control architectures and learning algorithms.
 * This software is published under the GNU Lesser General Public license (LGPL).
 *
 * It is not possible to add usage restrictions to an open-source license. Nevertheless,
 * we personally strongly object against this software being used by the military, in the
 * bio-industry, for animal experimentation, or anything that violates the Universal
 * Declaration of Human Rights.
 *
 * Copyright © 2012 Bart van Vliet <bart@almende.com>
 *
 * @author        Bart van Vliet
 * @date          Jul 25, 2012
 * @project       FireSwarm
 * @company       Distributed Organisms B.V.
 * @case          Swarm robots
 */

#include "CGroundStationSim.h"
#include "Protocol.h"
#include "Print.hpp"

using namespace rur;

CGroundStationSim::~CGroundStationSim()
{

}

void CGroundStationSim::Init(std::string module_id)
{
	groundStationSim::Init(module_id);
	ModuleId = module_id;
	UavId = UAVS_NUM;
	config.load("config.json");
	RadioRoundState = RADIO_STATE_ROUND_IDLE;
}

void CGroundStationSim::Tick()
{
	int* cmd = readCommand(false);
	if (cmd != NULL)
	{

	}

	VecMsg = readSim(false);
	if (!VecMsg->empty())
	{
		std::cout << "GroundStation " << ModuleId << " from SIM: ";
		dobots::print(VecMsg->begin(), VecMsg->end());

		VecMsgType::iterator it = VecMsg->begin();
		while (it != VecMsg->end())
		{
			int type = *it++;
			//std::cout << "Type=" << type << std::endl;
			switch (type)
			{
				case PROT_SIMCMD_RADIO_ROUND_START:
				{
					// Our turn to send messages
					RadioRoundState = RADIO_STATE_ROUND_START;
					WriteToRadio();
					break;
				}
				case PROT_SIMCMD_RADIO_ROUND_BROADCAST_MSG:
				{
					// Received messages from other uavs
					RadioRoundState = RADIO_STATE_ROUND_SENDRECEIVE;
					RadioMsg bmsg;
					it = FromCont(bmsg, it, VecMsg->end());
					std::cout << "GroundStation " << ModuleId << " received bmsg: " << bmsg << std::endl;
					ReceiveBuffer.push_back(bmsg);
					break;
				}
				case PROT_SIMCMD_RADIO_ROUND_END:
				{
					//RadioRoundState = RADIO_STATE_ROUND_END; // At this state we wait for other parts to update before going to IDLE
					ReadReceiveBuffer();
					RadioRoundState = RADIO_STATE_ROUND_IDLE; // At this state the write buffer can be filled with new msgs.

					VecMsgType vecMsg;
					vecMsg.push_back(PROT_SIMSTAT_ACK);
					std::cout << "to sim: ";
					dobots::print(vecMsg.begin(), vecMsg.end());
					writeToSim(vecMsg);
					break;
				}
			}
		}
		VecMsg->clear();

		// Send ack when we received the msgs from other uavs
		if (RadioRoundState == RADIO_STATE_ROUND_SENDRECEIVE)
		{
			VecMsgType vecMsg;
			vecMsg.push_back(PROT_SIMSTAT_ACK);
			std::cout << "to sim: ";
			dobots::print(vecMsg.begin(), vecMsg.end());
			writeToSim(vecMsg);
		}
	}

	usleep(config.TickTime);
}


void CGroundStationSim::ReadReceiveBuffer()
{
	UavStruct uav;
	while (!ReceiveBuffer.empty())
	{
		for (int i=0; i<RADIO_NUM_RELAY_PER_MSG; ++i)
		{
			switch (ReceiveBuffer.front().Data.Data[i].MessageType)
			{
				case RADIO_MSG_RELAY_POS:
				{
					uav.UavId = ReceiveBuffer.front().Data.Data[i].Pos.UavId -1;

					// Ignore invalid uav IDs and own messages
					if ((uav.UavId > -1) && (uav.UavId != UavId))
					{
						uav.FromRadioMsg(ReceiveBuffer.front().Data.Data[i]);
						std::cout << "GroundStation " << ModuleId << " received: " << ReceiveBuffer.front().Data.Data[i] << " === " << uav << std::endl;
					}
					break;
				}
				case RADIO_MSG_RELAY_FIRE:
				{
					// Add to fire map
					break;
				}
				case RADIO_MSG_RELAY_CMD:
				{
					// Ignore
					break;
				}
			}
		}
		ReceiveBuffer.pop_front();
	}
}

void CGroundStationSim::WriteToRadio()
{
	VecMsgType vecMsg;
	vecMsg.push_back(PROT_SIMSTAT_ACK);
	vecMsg.push_back(PROT_SIMSTAT_BROADCAST_MSG); // The radio msg to be broadcasted has to go last (after the ACK)!
	if (!SendBuffer.empty())
	{
		ToCont(SendBuffer.front(), vecMsg);
		SendBuffer.pop_front();
	}
	std::cout << "to sim: ";
	dobots::print(vecMsg.begin(), vecMsg.end());
	writeToSim(vecMsg);
}

void CGroundStationSim::WriteToOutBuffer(RadioMsg& msg)
{
	std::cout << "Requested msg to be sent: " << msg << std::endl;

	SendBuffer.push_back(msg);
}
