/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include "DDRace.h"

#include <engine/server.h>
#include <engine/server/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/pickup.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "algorithm"
#include "random"
#include "vector"

#define GAME_TYPE_NAME "DDraceNetwork"
#define TEST_TYPE_NAME "TestDDraceNetwork"
#define HIDDEN_TYPE_NAME "HiddenWorld"
#define HIDDEN_TEST_TYPE_NAME "TestHiddenWorld"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;

	// hidden mode
	InitTeleporter();
	if(HiddenModeCanTurnOn())
	{
		m_HiddenModeCanTurnOn = true;
		m_pGameType = g_Config.m_SvTestingCommands ? HIDDEN_TEST_TYPE_NAME : HIDDEN_TYPE_NAME;
	}
	srand((unsigned)time(NULL)); // 用当前时间作为种子
	for(auto &pHealth : m_Hidden.a_pHealthPointerList)
		pHealth = 0;
}

CGameControllerDDRace::~CGameControllerDDRace() = default;

CScore *CGameControllerDDRace::Score()
{
	return GameServer()->Score();
}

void CGameControllerDDRace::HandleCharacterTiles(CCharacter *pChr, int MapIndex)
{
	CPlayer *pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	int m_TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
	int m_TileFIndex = GameServer()->Collision()->GetFTileIndex(MapIndex);

	// Sensitivity
	int S1 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S2 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x + pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int S3 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y - pChr->GetProximityRadius() / 3.f));
	int S4 = GameServer()->Collision()->GetPureMapIndex(vec2(pChr->GetPos().x - pChr->GetProximityRadius() / 3.f, pChr->GetPos().y + pChr->GetProximityRadius() / 3.f));
	int Tile1 = GameServer()->Collision()->GetTileIndex(S1);
	int Tile2 = GameServer()->Collision()->GetTileIndex(S2);
	int Tile3 = GameServer()->Collision()->GetTileIndex(S3);
	int Tile4 = GameServer()->Collision()->GetTileIndex(S4);
	int FTile1 = GameServer()->Collision()->GetFTileIndex(S1);
	int FTile2 = GameServer()->Collision()->GetFTileIndex(S2);
	int FTile3 = GameServer()->Collision()->GetFTileIndex(S3);
	int FTile4 = GameServer()->Collision()->GetFTileIndex(S4);

	const int PlayerDDRaceState = pChr->m_DDRaceState;
	bool IsOnStartTile = (m_TileIndex == TILE_START) || (m_TileFIndex == TILE_START) || FTile1 == TILE_START || FTile2 == TILE_START || FTile3 == TILE_START || FTile4 == TILE_START || Tile1 == TILE_START || Tile2 == TILE_START || Tile3 == TILE_START || Tile4 == TILE_START;
	// start
	if(IsOnStartTile && PlayerDDRaceState != DDRACE_CHEAT)
	{
		const int Team = GameServer()->GetDDRaceTeam(ClientID);
		if(Teams().GetSaving(Team))
		{
			GameServer()->SendStartWarning(ClientID, "You can't start while loading/saving of team is in progress");
			pChr->Die(ClientID, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam == SV_TEAM_MANDATORY && (Team == TEAM_FLOCK || Teams().Count(Team) <= 1))
		{
			GameServer()->SendStartWarning(ClientID, "You have to be in a team with other tees to start");
			pChr->Die(ClientID, WEAPON_WORLD);
			return;
		}
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team > TEAM_FLOCK && Team < TEAM_SUPER && Teams().Count(Team) < g_Config.m_SvMinTeamSize)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Your team has fewer than %d players, so your team rank won't count", g_Config.m_SvMinTeamSize);
			GameServer()->SendStartWarning(ClientID, aBuf);
		}
		if(g_Config.m_SvResetPickups)
		{
			pChr->ResetPickups();
		}

		Teams().OnCharacterStart(ClientID);
		pChr->m_LastTimeCp = -1;
		pChr->m_LastTimeCpBroadcasted = -1;
		for(float &CurrentTimeCp : pChr->m_aCurrentTimeCp)
		{
			CurrentTimeCp = 0.0f;
		}
	}

	// finish
	if(((m_TileIndex == TILE_FINISH) || (m_TileFIndex == TILE_FINISH) || FTile1 == TILE_FINISH || FTile2 == TILE_FINISH || FTile3 == TILE_FINISH || FTile4 == TILE_FINISH || Tile1 == TILE_FINISH || Tile2 == TILE_FINISH || Tile3 == TILE_FINISH || Tile4 == TILE_FINISH) && PlayerDDRaceState == DDRACE_STARTED)
		Teams().OnCharacterFinish(ClientID);

	// unlock team
	else if(((m_TileIndex == TILE_UNLOCK_TEAM) || (m_TileFIndex == TILE_UNLOCK_TEAM)) && Teams().TeamLocked(GameServer()->GetDDRaceTeam(ClientID)))
	{
		Teams().SetTeamLock(GameServer()->GetDDRaceTeam(ClientID), false);
		GameServer()->SendChatTeam(GameServer()->GetDDRaceTeam(ClientID), "Your team was unlocked by an unlock team tile");
	}

	// solo part
	if(((m_TileIndex == TILE_SOLO_ENABLE) || (m_TileFIndex == TILE_SOLO_ENABLE)) && !Teams().m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now in a solo part");
		pChr->SetSolo(true);
	}
	else if(((m_TileIndex == TILE_SOLO_DISABLE) || (m_TileFIndex == TILE_SOLO_DISABLE)) && Teams().m_Core.GetSolo(ClientID))
	{
		GameServer()->SendChatTarget(ClientID, "You are now out of the solo part");
		pChr->SetSolo(false);
	}
}

// 玩家进入服务器
void CGameControllerDDRace::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	int ClientID = pPlayer->GetCID();

	// init the player
	Score()->PlayerData(ClientID)->Reset();

	// Can't set score here as LoadScore() is threaded, run it in
	// LoadScoreThreaded() instead
	Score()->LoadPlayerData(ClientID);

	if(!Server()->ClientPrevIngame(ClientID))
	{
		// 进入消息
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "'%s' entered and joined the %s", Server()->ClientName(ClientID), GetTeamName(pPlayer->GetTeam()));
		// GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);
		GameServer()->SendChatTarget(-1, aBuf);
		GameServer()->SendChatTarget(ClientID, "Hidden Mode 服务器版本: " GAME_VERSION);
	}
}

// 玩家退出服务器
void CGameControllerDDRace::OnPlayerDisconnect(CPlayer *pPlayer, const char *pReason)
{
	int ClientID = pPlayer->GetCID();
	bool WasModerator = pPlayer->m_Moderating && Server()->ClientIngame(ClientID);

	IGameController::OnPlayerDisconnect(pPlayer, pReason);

	if(!GameServer()->PlayerModerating() && WasModerator)
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");

	if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO)
		Teams().SetForceCharacterTeam(ClientID, TEAM_FLOCK);
}

void CGameControllerDDRace::OnReset()
{
	IGameController::OnReset();
	Teams().Reset();
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();
	Teams().ProcessSaveTeam();
	Teams().Tick();

	if(m_pLoadBestTimeResult != nullptr && m_pLoadBestTimeResult->m_Completed)
	{
		if(m_pLoadBestTimeResult->m_Success)
		{
			m_CurrentRecord = m_pLoadBestTimeResult->m_CurrentRecord;

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetClientVersion() >= VERSION_DDRACE)
				{
					GameServer()->SendRecord(i);
				}
			}
		}
		m_pLoadBestTimeResult = nullptr;
	} // 以上是原本的代码

	if(m_HiddenModeCanTurnOn)
	{
		if(m_Hidden.stepEndTick != -1)
		{ // Hidden Mode相关Tick处理
			int nowTick = Server()->Tick();
			int tickSpeed = Server()->TickSpeed();
			int endTick = m_Hidden.stepEndTick;

			HiddenTick(nowTick, endTick, tickSpeed, m_Hidden.nowStep);
		}
	}

	// 假人设备机器保活
	if(Server()->Tick() % 500 == 0) // 机器人每10s进行一次保活操作(50tick * 10)
	{
		CServer *pServer = (CServer *)Server();
		for(int i = 0; i < m_Hidden.deviceNum; i++)
		{
			if(!GameServer()->m_apPlayers[0])
				break; // 假人已被踢出，不能继续了
			pServer->m_NetServer.m_aSlots[i].m_Connection.HiddenFreshLastRecvTime();
			pServer->m_NetServer.m_aSlots[i].m_Connection.HiddenFreshFirstSendTime();
		}
	}
}

void CGameControllerDDRace::HiddenTick(int nowTick, int endTick, int tickSpeed, int nowStep)
{
	char aBuf[256];
	// 本阶段剩余时间
	double remainTime = static_cast<double>(endTick - nowTick) / tickSpeed;
	if(remainTime) // 仅防止未使用变量警告
	{
	}
	// 本阶段剩余Tick
	int remainTick = endTick - nowTick;
	bool isShowRemainTime =
		(remainTick % (30 * tickSpeed) == 0) ||
		(remainTick < tickSpeed * 20 && remainTick % (5 * tickSpeed) == 0) ||
		(remainTick < tickSpeed * 4 && remainTick % (1 * tickSpeed) == 0);

	if(remainTick)
		// 全局倒计时消息
		if(isShowRemainTime)
		{
			str_format(aBuf, sizeof(aBuf), "%s %.2f %s", Config()->m_HiddenTimeLeftMSGPrefix, remainTime, Config()->m_HiddenTimeLeftMSGSuffix);
			GameServer()->SendChatTarget(-1, aBuf);
		}

	switch(nowStep)
	{
	case STEP_S0:
	{
		if(remainTick)
			if(isShowRemainTime)
			{ // 剩余时间提示
				str_format(aBuf, sizeof(aBuf), "%s %.2f %s", Config()->m_HiddenTimeLeftStartPrefix, remainTime, Config()->m_HiddenTimeLeftStartSuffix);
				GameServer()->SendBroadcast(aBuf, -1);
			}

		if(nowTick == endTick)
		{
			// 判断人数
			int playerCount = 0;
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(this->HiddenIsMachine(pPlayer))
					continue; // 机器人不计算在内
				if(pPlayer->IsAfk())
					continue; // 挂机玩家不计算在内
				playerCount++;
			}

			if(playerCount > 1)
				GameServer()->CallVote(0, Config()->m_HiddenAutoStartDesc, Config()->m_HiddenAutoStartCmd, Config()->m_HiddenAutoStartReason, Config()->m_HiddenAutoStartChatmsg);
			else
			{
				str_format(aBuf, sizeof(aBuf), "%s %d %s", Config()->m_HiddenNotEnoughPlayersMSGPrefix, playerCount, Config()->m_HiddenNotEnoughPlayersMSGSuffix);
				GameServer()->SendBroadcast(aBuf, -1);
			}
			m_Hidden.stepEndTick = nowTick + tickSpeed * Config()->m_HiddenStepDurationS0;
		}
		break;
	};
	case STEP_S5:
	{
		if(nowTick == endTick)
		{
			HiddenRemoveHealthPointer(-1); // 关闭指南针
			printf(">>> STEP_S5 DONE\n");
			HiddenStepUpdate(STEP_S0);
		}
		break;
	}
	case STEP_S1:
	{
		int vote201Num = 0;
		int vote202Num = 0;
		static int lastVote201Num = 0;
		static int lastVote202Num = 0;

		int tele201 = 201;
		int tele202 = 202;

		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;
			int resTele = HiddenGetPlayerVoteByTelePos(pPlayer, tele201, tele202);
			if(resTele == tele201)
			{
				vote201Num++;
			}
			else if(resTele == tele202)
			{
				vote202Num++;
			}
		}

		if(vote201Num != lastVote201Num || vote202Num != lastVote202Num)
		{
			str_format(aBuf, sizeof(aBuf), "%s:%d \t\t\t %s:%d", Config()->m_HiddenStepVoteS1A, vote201Num, Config()->m_HiddenStepVoteS1B, vote202Num);
			GameServer()->SendBroadcast(aBuf, -1);

			lastVote201Num = vote201Num;
			lastVote202Num = vote202Num;
		}

		if(nowTick == endTick)
		{ // 选择时间结束，开始判断
			printf(">>> STEP_S1 DONE\n");

			m_Hidden.stepDurationTime = -1;
			if(vote201Num > vote202Num)
			{ // 同意
				HiddenStepUpdate(STEP_S2);
				str_format(aBuf, sizeof(aBuf), "%s%s", Config()->m_HiddenStepVoteResultMSG, Config()->m_HiddenStepVoteS1AValue);
			}
			else
			{ // 拒绝
				HiddenStepUpdate(STEP_S0);
				str_format(aBuf, sizeof(aBuf), "%s%s", Config()->m_HiddenStepVoteResultMSG, Config()->m_HiddenStepVoteS1BValue);
			}
			GameServer()->SendChatTarget(-1, aBuf);
		}
		break;
	}
	case STEP_S2:
	{
		int vote211Num = 0;
		int vote212Num = 0;
		int vote213Num = 0;
		int vote214Num = 0;
		static int lastVote211Num = 0;
		static int lastVote212Num = 0;
		static int lastVote213Num = 0;
		static int lastVote214Num = 0;

		int tele211 = 211;
		int tele212 = 212;
		int tele213 = 213;
		int tele214 = 214;

		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;
			int resTele = HiddenGetPlayerVoteByTelePos(pPlayer, tele211, tele212, tele213, tele214);

			if(resTele == tele211)
			{
				vote211Num++;
			}
			else if(resTele == tele212)
			{
				vote212Num++;
			}
			else if(resTele == tele213)
			{
				vote213Num++;
			}
			else if(resTele == tele214)
			{
				vote214Num++;
			}
		}

		if(lastVote211Num != vote211Num || lastVote212Num != vote212Num || lastVote213Num != vote213Num || lastVote214Num != vote214Num)
		{
			str_format(aBuf, sizeof(aBuf), "%s:%d \t %s:%d \t %s:%d \t %s:%d", Config()->m_HiddenStepVoteS2A, vote211Num, Config()->m_HiddenStepVoteS2B, vote212Num, Config()->m_HiddenStepVoteS2C, vote213Num, Config()->m_HiddenStepVoteS2D, vote214Num);
			GameServer()->SendBroadcast(aBuf, -1);

			lastVote211Num = vote211Num;
			lastVote212Num = vote212Num;
			lastVote213Num = vote213Num;
			lastVote214Num = vote214Num;
		}

		if(nowTick == endTick)
		{ // 选择时间结束，开始判断
			printf(">>> STEP_S2 DONE\n");

			m_Hidden.stepDurationTime = -1;
			if(vote213Num > vote212Num && vote213Num > vote214Num)
			{
				m_Hidden.seekerNum = Config()->m_HiddenStepVoteS2CValue;
			}
			else if(vote214Num > vote213Num && vote214Num > vote212Num)
			{
				m_Hidden.seekerNum = Config()->m_HiddenStepVoteS2DValue;
			}
			else
			{
				m_Hidden.seekerNum = Config()->m_HiddenStepVoteS2BValue;
			}

			str_format(aBuf, sizeof(aBuf), "%s%d", Config()->m_HiddenStepVoteResultMSG, m_Hidden.seekerNum);
			GameServer()->SendChatTarget(-1, aBuf);

			if(m_Hidden.seekerNum >= m_Hidden.iS1PlayerNum)
			{ // 玩家数量少于选择的猎人数量
				HiddenStepUpdate(STEP_S0);
				GameServer()->SendBroadcast(Config()->m_HiddenStepVoteResultMSGTOOManySeekers, -1);
			}
			else
			{ // 猎人数量正确
				HiddenStepUpdate(STEP_S3);
			}
		}
		break;
	}
	case STEP_S3:
	{
		int vote221Num = 0;
		int vote222Num = 0;
		int vote223Num = 0;
		int vote224Num = 0;
		static int lastVote221Num = 0;
		static int lastVote222Num = 0;
		static int lastVote223Num = 0;
		static int lastVote224Num = 0;

		int tele221 = 221;
		int tele222 = 222;
		int tele223 = 223;
		int tele224 = 224;

		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;
			int resTele = HiddenGetPlayerVoteByTelePos(pPlayer, tele221, tele222, tele223, tele224);

			if(resTele == tele221)
			{
				vote221Num++;
			}
			else if(resTele == tele222)
			{
				vote222Num++;
			}
			else if(resTele == tele223)
			{
				vote223Num++;
			}
			else if(resTele == tele224)
			{
				vote224Num++;
			}
		}

		if(lastVote221Num != vote221Num || lastVote222Num != vote222Num || lastVote223Num != vote223Num || lastVote224Num != vote224Num)
		{
			str_format(aBuf, sizeof(aBuf), "%s:%d \t %s:%d \t %s:%d \t %s:%d", Config()->m_HiddenStepVoteS3A, vote221Num, Config()->m_HiddenStepVoteS3B, vote222Num, Config()->m_HiddenStepVoteS3C, vote223Num, Config()->m_HiddenStepVoteS3D, vote224Num);
			GameServer()->SendBroadcast(aBuf, -1);

			lastVote221Num = vote221Num;
			lastVote222Num = vote222Num;
			lastVote223Num = vote223Num;
			lastVote224Num = vote224Num;
		}

		if(nowTick == endTick)
		{ // 选择时间结束，开始判断
			printf(">>> STEP_S3 DONE\n");

			m_Hidden.stepDurationTime = -1;
			if(vote223Num > vote222Num && vote223Num > vote224Num)
			{
				m_Hidden.machineNum = Config()->m_HiddenStepVoteS3CValue;
			}
			else if(vote224Num > vote223Num && vote224Num > vote222Num)
			{
				m_Hidden.machineNum = Config()->m_HiddenStepVoteS3DValue;
			}
			else
			{
				m_Hidden.machineNum = Config()->m_HiddenStepVoteS3BValue;
			}
			HiddenStepUpdate(STEP_S4);
			// 投票结果: <机器数量>
			str_format(aBuf, sizeof(aBuf), "%s%d", Config()->m_HiddenStepVoteResultMSG, m_Hidden.machineNum);
			GameServer()->SendChatTarget(-1, aBuf);
		}
		break;
	}
	case STEP_S4:
	{
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{ // 玩家状态更新检测
			if(!pPlayer)
				continue;
			if(pPlayer->m_Hidden.m_IsDummyMachine)
				continue;

			bool isInGame = pPlayer->m_Hidden.m_InGame;
			bool isBeenKilled = pPlayer->m_Hidden.m_HasBeenKilled;
			bool isInSpectator = pPlayer->GetTeam() == TEAM_SPECTATORS;
			bool isLose = pPlayer->m_Hidden.m_IsLose;
			bool isAFK = pPlayer->IsAfk();

			if(isInGame && isBeenKilled && !isInSpectator && !isLose)
			{ // 玩家被猎人锤中
				// 该玩家标记为失败
				pPlayer->m_Hidden.m_IsLose = true;
				// 受害者移动到旁观列表
				pPlayer->SetTeam(TEAM_SPECTATORS, false);

				// 存活玩家数量(猎人+求生者)
				int alivePlayerNum = 0;
				for(auto &pPs : GameServer()->m_apPlayers)
				{
					if(HiddenIsPlayerGameOver(pPs))
						continue;
					alivePlayerNum++;
				}

				// 全局广播	受害者出局
				str_format(aBuf, sizeof(aBuf), "%s %s", Server()->ClientName(pPlayer->GetCID()), Config()->m_HiddenStepPlayerGameOverMSG); // <playername>出局了!
				GameServer()->SendBroadcast(aBuf, -1);
				// 聊天消息
				str_format(aBuf, sizeof(aBuf), "%s:%s", Server()->ClientName(pPlayer->GetCID()), Config()->m_HiddenStepPlayerGameOverChatMSG);
				GameServer()->SendChatTarget(-1, aBuf);
				str_format(aBuf, sizeof(aBuf), "%s:%d", Config()->m_HiddenStepPlayerGameOverChatMSG2, alivePlayerNum); // 剩余人数:<num>
				GameServer()->SendChatTarget(-1, aBuf);
			}
			else if(!isInGame && !isInSpectator)
			{ // 新入服务器玩家
				// 移动到旁观列表
				pPlayer->SetTeam(TEAM_SPECTATORS, false);

				// 个人广播	下一轮加入
				str_format(aBuf, sizeof(aBuf), "%s %s", Server()->ClientName(pPlayer->GetCID()), Config()->m_HiddenStepPlayerWaitingMSG);
				GameServer()->SendBroadcast(aBuf, pPlayer->GetCID());
				// 聊天消息
				str_format(aBuf, sizeof(aBuf), "%s:%s", Server()->ClientName(pPlayer->GetCID()), Config()->m_HiddenStepPlayerWaitingMSG);
				GameServer()->SendChatTarget(pPlayer->GetCID(), aBuf);
			}
			else if(
				(!isInSpectator && isLose) ||
				isAFK)
			{ // 其余奇葩情况
				// 移动到旁观列表
				pPlayer->SetTeam(TEAM_SPECTATORS, false);
			}
		}

		// 人数计算
		int seekerNum = 0; // 猎人数量
		int hiderNum = 0; // 求生者数量
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;

			if(pPlayer->m_Hidden.m_IsSeeker)
				seekerNum++;
			else
				hiderNum++;
		}

		if(seekerNum == 0 && hiderNum == 0)
		{ // 异常
			GameServer()->SendBroadcast(Config()->m_HiddenStepPlayerNumErrorMSG, -1);
			HiddenStepUpdate(STEP_S0);
			break;
		}

		if(hiderNum == 0)
		{ // 求生者全死 --> 猎人赢了
			this->m_Hidden.whoWin = SEEKER_WIN;
		}
		else if(seekerNum == 0 || m_Hidden.activedMachine >= m_Hidden.machineNum)
		{ // 机器全部激活 --> 求生者赢了
			this->m_Hidden.whoWin = HIDER_WIN;
		}

		if(m_Hidden.whoWin > NONE_WIN)
		{ // 有一方胜利
			// 游戏结束
			HiddenStepUpdate(STEP_S5);
			printf(">>> STEP_S4 DONE\n");
			break;
		}

		if(nowTick == endTick - tickSpeed * 90 ||
			nowTick == endTick - tickSpeed * 60 ||
			nowTick == endTick - tickSpeed * 30 ||
			nowTick == endTick - tickSpeed * 15 ||
			nowTick == endTick - tickSpeed * 5 ||
			nowTick == endTick - tickSpeed * 3 ||
			nowTick == endTick - tickSpeed * 2 ||
			nowTick == endTick - tickSpeed * 1)
		{ // 机器剩余激活时间提示
			double tipRemainTime = (double)(endTick - nowTick) / tickSpeed;
			str_format(aBuf, sizeof(aBuf), "%s %.2f %s", Config()->m_HiddenStepLeftTimeToActiveDeviceMSGPrefix, tipRemainTime, Config()->m_HiddenStepLeftTimeToActiveDeviceMSGSuffix);
			GameServer()->SendBroadcast(aBuf, -1);
		}
		// health指南针判断
		// 游戏时长是否超过了hidden_duration_s4_normal时间
		// 如果超过则全员启动指南针，否则仅在倒计时15秒时启动
		if(nowTick >= m_Hidden.stepStartTick + tickSpeed * Config()->m_HiddenStepDurationS4Normal)
		{
			// 启动指南针
			HiddenCreateHealthPointer(-1);
		}
		else if(nowTick >= endTick - tickSpeed * 15)
		{
			// 启动指南针
			HiddenCreateHealthPointer(-1);
		}
		else if(nowTick < endTick - tickSpeed * 15)
		{
			// 关闭指南针
			HiddenRemoveHealthPointer(-1);
		}

		if(nowTick == endTick)
		{ // 求生者没有在规定时间内激活机器
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{ // 所有还在游戏中的求生者设置为被killed状态
				if(!pPlayer)
					continue;

				if(HiddenIsPlayerGameOver(pPlayer))
					continue;
				if(pPlayer->m_Hidden.m_IsSeeker)
					continue;

				pPlayer->m_Hidden.m_HasBeenKilled = true;
			}
		}
		break;
	}
	}
}

/*
	玩家游戏结束了吗？
 */
bool CGameControllerDDRace::HiddenIsPlayerGameOver(CPlayer *pPlayer)
{
	if(!pPlayer)
		return true; // 没有玩家
	if(!pPlayer->GetCharacter())
		return true; // 没有生成角色

	if(HiddenIsMachine(pPlayer))
		return true; // 是假人机器设备
	if(pPlayer->GetTeam() == TEAM_SPECTATORS)
		return true; // 在旁观模式
	if(pPlayer->IsAfk())
		return true; // 在挂机

	if(pPlayer->m_Hidden.m_HasBeenKilled)
		return true; // 已被杀死
	if(pPlayer->m_Hidden.m_InGame == false)
		return true; // 未加入游戏
	if(pPlayer->m_Hidden.m_IsLose)
		return true; // 已标记为失败

	return false;
}
/*
	是假人机器设备
 */
bool CGameControllerDDRace::HiddenIsMachine(CPlayer *pPlayer)
{
	if(!pPlayer)
		return true; // 没有玩家

	if(pPlayer->m_Hidden.m_IsDummyMachine)
		return true; // 是机器(假人、设备)
	if(pPlayer->m_Hidden.m_IsPlaceholder)
		return true; // 是占位机器人

	return false;
}
/*
	阶段更新
 */
void CGameControllerDDRace::HiddenStepUpdate(int toStep)
{
	int tickSpeed = Server()->TickSpeed();
	int tickNow = Server()->Tick();
	switch(toStep)
	{
	case STEP_S0:
	{ // 大厅
		printf("STEP_S0: init setup\n");
		// 如果没有到S4，则玩家重生
		if(m_Hidden.nowStep < STEP_S4)
		{
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(HiddenIsMachine(pPlayer))
					continue;

				pPlayer->SetTeam(TEAM_FLOCK, false);
				pPlayer->TryRespawn();
				pPlayer->m_SpectatorID = SPEC_FREEVIEW;
			}
		}
		else
		{ // 大于等于S4 --> 假人(设备、机器)归位
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(!HiddenIsMachine(pPlayer))
					continue; // 是玩家

				HiddenTeleportPlayerToCheckPoint(pPlayer, 241);
			}
		}

		if(m_Hidden.whoWin != NONE_WIN)
		{ // 冠军房间传送
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(HiddenIsMachine(pPlayer))
					continue; // 假人不传送
				if(pPlayer->m_Hidden.m_InGame == false)
					continue; // 未加入游戏玩家不传送

				if(pPlayer->m_Hidden.m_IsWin)
					HiddenTeleportPlayerToCheckPoint(pPlayer, 251);
				else
					HiddenTeleportPlayerToCheckPoint(pPlayer, 252);
			}
		}

		HiddenStateReset();
		m_Hidden.stepDurationTime = Config()->m_HiddenStepDurationS0;
		m_Hidden.stepStartTick = tickNow;
		m_Hidden.stepEndTick = tickNow + tickSpeed * m_Hidden.stepDurationTime;

		// 关闭Hidden Mode
		GameServer()->HiddenModeStop();
		printf("STEP_S0: init done\n");
		break;
	}
	case STEP_S1:
	{ // 玩家传送到S1 开始游戏房间
		printf("STEP_S1: init setup\n");
		int iPlayerNum = 0;
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;

			HiddenTeleportPlayerToCheckPoint(pPlayer, 200);
			iPlayerNum++;
			// 分数重置
			pPlayer->m_Score = 0;
		}
		m_Hidden.nowStep = STEP_S1;
		m_Hidden.stepDurationTime = GameServer()->Config()->m_HiddenStepDurationS1;
		m_Hidden.stepStartTick = tickNow;
		m_Hidden.stepEndTick = tickNow + tickSpeed * m_Hidden.stepDurationTime;
		m_Hidden.iS1PlayerNum = iPlayerNum;

		printf("STEP_S1: init done\n");
		break;
	}
	case STEP_S2:
	{ // 玩家传送到S2 猎人数量房间
		printf("STEP_S2: init setup\n");
		// 禁止旁观
		m_Hidden.canZoom = false;

		CGameControllerDDRace *pController = (CGameControllerDDRace *)GameServer()->m_pController;
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;
			// 取消旁观
			pPlayer->Pause(CPlayer::PAUSE_NONE, true);
			pPlayer->m_Hidden.m_isFirstEnterGame = false; // 不是第一次进入游戏
			pController->HiddenTeleportPlayerToCheckPoint(pPlayer, 210);
		}
		// 传送假人当配置提示
		HiddenTeleportPlayerToCheckPoint(GameServer()->m_apPlayers[0], 212);
		HiddenTeleportPlayerToCheckPoint(GameServer()->m_apPlayers[1], 213);
		HiddenTeleportPlayerToCheckPoint(GameServer()->m_apPlayers[2], 214);

		m_Hidden.nowStep = STEP_S2;
		m_Hidden.stepDurationTime = GameServer()->Config()->m_HiddenStepDurationS2;
		m_Hidden.stepStartTick = tickNow;
		m_Hidden.stepEndTick = tickNow + tickSpeed * m_Hidden.stepDurationTime;

		printf("STEP_S2: init done\n");
		break;
	}
	case STEP_S3:
	{ // 玩家传送到S3 机器数量房间
		printf("STEP_S3: init setup\n");
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;
			HiddenTeleportPlayerToCheckPoint(pPlayer, 220);
		}
		// 传送假人当配置提示
		HiddenTeleportPlayerToCheckPoint(GameServer()->m_apPlayers[0], 222);
		HiddenTeleportPlayerToCheckPoint(GameServer()->m_apPlayers[1], 223);
		HiddenTeleportPlayerToCheckPoint(GameServer()->m_apPlayers[2], 224);

		m_Hidden.nowStep = STEP_S3;
		m_Hidden.stepDurationTime = GameServer()->Config()->m_HiddenStepDurationS3;
		m_Hidden.stepStartTick = tickNow;
		m_Hidden.stepEndTick = tickNow + tickSpeed * m_Hidden.stepDurationTime;

		printf("STEP_S3: init done\n");
		break;
	}
	case STEP_S4:
	{ // 玩家传送到S4 正式游戏房间
		printf("STEP_S4: init setup\n");
		// 随机选取猎人
		std::vector<int> randomVector = unique_random_numbers(m_Hidden.iS1PlayerNum, m_Hidden.seekerNum);
		printf("猎人分配如下: ");
		int currentIndex = 0;
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsPlayerGameOver(pPlayer))
				continue;

			for(int i = 0; i < (int)(randomVector.size()); i++)
			{
				if(randomVector[i] == currentIndex)
				{
					pPlayer->m_Hidden.m_IsSeeker = true;
					break;
				}
			}

			if(pPlayer->m_Hidden.m_IsSeeker)
			{ // 玩家是猎人
				HiddenTeleportPlayerToCheckPoint(pPlayer, 232);
				printf("%s ", Server()->ClientName(pPlayer->GetCID()));
				GameServer()->SendBroadcast(Config()->m_HiddenStepTipsS4A1, pPlayer->GetCID());
				GameServer()->WhisperID(0, pPlayer->GetCID(), Config()->m_HiddenStepTipsS4A2);
				pPlayer->m_Score = 1000;
			}
			else
			{ // 不是猎人
				HiddenTeleportPlayerToCheckPoint(pPlayer, 231);
				GameServer()->SendBroadcast(Config()->m_HiddenStepTipsS4B1, pPlayer->GetCID());
				GameServer()->WhisperID(0, pPlayer->GetCID(), Config()->m_HiddenStepTipsS4B2);
				pPlayer->m_Score = 0;
			}

			currentIndex++;
		}
		printf("\n");
		// 输出投票结果
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s%s %d %s %d",
			Config()->m_HiddenStepVoteResultConclusion,
			Config()->m_HiddenStepVoteResultConclusionHiderPrefix,
			this->m_Hidden.iS1PlayerNum - this->m_Hidden.seekerNum,
			Config()->m_HiddenStepVoteResultConclusionSeekerPrefix,
			this->m_Hidden.seekerNum);
		GameServer()->SendChatTarget(-1, aBuf);

		// 机器(假人、设备)开始运作
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(!pPlayer)
				continue;
			if(!pPlayer->m_Hidden.m_IsDummyMachine)
				continue; // 不是机器人

			HiddenTeleportPlayerToCheckPoint(pPlayer, 242);
		}

		// 开始游戏
		HiddenStartGame();

		m_Hidden.nowStep = STEP_S4;
		m_Hidden.stepDurationTime = GameServer()->Config()->m_HiddenStepDurationS4;
		m_Hidden.stepStartTick = tickNow;
		m_Hidden.stepEndTick = tickNow + tickSpeed * m_Hidden.stepDurationTime;

		printf("STEP_S4: init done\n");
		break;
	}
	case STEP_S5:
	{ // 结束游戏
		printf("STEP_S5: init setup\n");
		char aBuf[256];
		CGameControllerDDRace *pController = (CGameControllerDDRace *)GameServer()->m_pController;
		if(pController->m_Hidden.whoWin == SEEKER_WIN)
		{ // 猎人胜利标记
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(HiddenIsPlayerGameOver(pPlayer))
					continue; // 不在游戏中

				if(pPlayer->m_Hidden.m_IsSeeker)
					pPlayer->m_Hidden.m_IsWin = true;
			}
			str_copy(aBuf, Config()->m_HiddenSeekerWin);
		}
		else if(pController->m_Hidden.whoWin == HIDER_WIN)
		{ // 求生者胜利标记
			for(auto &pPlayer : GameServer()->m_apPlayers)
			{
				if(HiddenIsPlayerGameOver(pPlayer))
					continue; // 不在游戏中

				if(!pPlayer->m_Hidden.m_IsSeeker)
					pPlayer->m_Hidden.m_IsWin = true;
			}
			str_copy(aBuf, Config()->m_HiddenHiderWin);
		}

		// 只保留最后一次行动的玩家，其他玩家都旁观他
		for(auto &pPlayer : GameServer()->m_apPlayers)
		{
			if(HiddenIsMachine(pPlayer))
				continue; // 排除机器人
			if(pPlayer->GetCID() == m_Hidden.lastActiveClientID)
				continue; // 排除最后一次行动玩家

			pPlayer->SetTeam(TEAM_SPECTATORS, false);
			pPlayer->m_SpectatorID = m_Hidden.lastActiveClientID;
		}

		// 消息广播
		GameServer()->SendBroadcast(aBuf, -1);

		m_Hidden.nowStep = STEP_S5;
		m_Hidden.stepDurationTime = 4;
		m_Hidden.stepStartTick = tickNow;
		m_Hidden.stepEndTick = tickNow + tickSpeed * m_Hidden.stepDurationTime;

		printf("STEP_S5: init done\n");
		break;
	}
	}
}

void CGameControllerDDRace::HiddenTeleportPlayerToCheckPoint(CPlayer *pPlayer, int TeleTo)
{
	if(!pPlayer)
		return;

	CCharacter *pChr = pPlayer->GetCharacter();
	if(!m_TeleOuts[TeleTo - 1].empty())
	{
		int TeleOutIndex = GameServer()->m_World.m_Core.RandomOr0(m_TeleOuts[TeleTo - 1].size());
		if(pChr)
		{
			HiddenTeleportPlayerToPosition(pChr, m_TeleOuts[TeleTo - 1][TeleOutIndex]);
		}
		else
		{
			vec2 TeleOutPos = m_TeleOuts[TeleTo - 1][TeleOutIndex];
			pPlayer->ForceSpawn(TeleOutPos);
		}
	}
}

// 传送角色到Pos
void CGameControllerDDRace::HiddenTeleportPlayerToPosition(CCharacter *pChr, vec2 Pos)
{
	pChr->SetPosition(Pos);
	pChr->m_Pos = Pos;
	pChr->m_PrevPos = Pos;
}
void CGameControllerDDRace::HiddenPauseGame(bool isPause)
{
	GameServer()->m_World.m_Paused = isPause;
}
void CGameControllerDDRace::HiddenStartGame()
{
	IGameController::m_RoundStartTick = Server()->Tick();
	m_SuddenDeath = 0;
	m_GameOverTick = -1;
}
void CGameControllerDDRace::HiddenCreateHealthPointer(int clientID)
{
	int max = MAX_CLIENTS;
	int min = 0;
	if(clientID == -1) // 全局增加指南针
	{
		if(m_Hidden.isCreatedGlobalHealthList)
			return;
		m_Hidden.isCreatedGlobalHealthList = true;
	}
	else // 个人
	{
		max = clientID + 1;
		min = clientID;
	}
	for(int i = min; i < max; i++)
	{
		if(m_Hidden.a_pHealthPointerList[i])
			continue;
		auto *pPlayer = GameServer()->m_apPlayers[i];
		if(HiddenIsMachine(pPlayer))
			continue;
		if(!pPlayer->GetCharacter())
			continue;
		CPickup *pPickup = new CPickup(&GameServer()->m_World, POWERUP_HEALTH, HIDDEN_POWERUP_HEALTH);
		pPickup->m_HiddenBindPlayerClient = pPlayer->GetCID();
		m_Hidden.a_pHealthPointerList[i] = pPickup;

		// whisper
		// 私聊通知内容：指南针已开启
		GameServer()->WhisperID(0, pPlayer->GetCID(), Config()->m_HiddenStepDeviceCompassActivatedWhisperMSG);
	}
}
void CGameControllerDDRace::HiddenRemoveHealthPointer(int clientID)
{
	int max = MAX_CLIENTS;
	int min = 0;
	if(clientID == -1)
	{
		if(!m_Hidden.isCreatedGlobalHealthList)
			return;
		m_Hidden.isCreatedGlobalHealthList = false;
	}
	else
	{
		max = clientID + 1;
		min = clientID;
	}
	for(int i = min; i < max; i++)
	{
		CPickup *pHealth = m_Hidden.a_pHealthPointerList[i];
		if(!pHealth)
			continue;
		GameServer()->m_World.RemoveEntity(pHealth);
		m_Hidden.a_pHealthPointerList[i] = nullptr;
	}
}

void CGameControllerDDRace::DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg)
{
	Team = ClampTeam(Team);
	if(Team == pPlayer->GetTeam())
		return;

	CCharacter *pCharacter = pPlayer->GetCharacter();

	if(Team == TEAM_SPECTATORS)
	{
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && pCharacter)
		{
			// Joining spectators should not kill a locked team, but should still
			// check if the team finished by you leaving it.
			int DDRTeam = pCharacter->Team();
			Teams().SetForceCharacterTeam(pPlayer->GetCID(), TEAM_FLOCK);
			Teams().CheckTeamFinished(DDRTeam);
		}
	}

	IGameController::DoTeamChange(pPlayer, Team, DoChatMsg);
}

template<typename... Args>
int CGameControllerDDRace::HiddenGetPlayerVoteByTelePos(CPlayer *pPlayer, Args... args)
{
	// 将可变参数转换为一个数组
	int aTelePos[] = {args...};
	int voteTele = aTelePos[0];

	if(!pPlayer)
		return voteTele;
	if(!pPlayer->GetCharacter())
		return voteTele;

	CGameControllerDDRace *pController = (CGameControllerDDRace *)GameServer()->m_pController;
	vec2 vPlayerPos = pPlayer->GetCharacter()->GetPos();

	// 遍历数组中的每个元素
	double minDistance = INFINITY;
	for(int i = 0; i < int(sizeof(aTelePos) / sizeof(int)); i++)
	{
		vec2 telePos = pController->m_TeleOuts[aTelePos[i] - 1][0];
		double dis = distance(vPlayerPos, telePos);
		if(dis < minDistance)
		{
			minDistance = dis;
			voteTele = aTelePos[i];
		}
	}
	return voteTele;
}