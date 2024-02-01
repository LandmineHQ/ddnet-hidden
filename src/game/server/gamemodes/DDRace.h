/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_GAMEMODES_DDRACE_H
#define GAME_SERVER_GAMEMODES_DDRACE_H

#include <game/server/entities/character.h>
#include <game/server/entities/pickup.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

// Hidden Mode各阶段状态枚举
enum
{
	STEP_S0 = 0, // 大厅
	STEP_S1, // 开始游戏房间
	STEP_S2, // 猎人数量房间
	STEP_S3, // 机器数量房间
	STEP_S4, // 游戏进行房间
	STEP_S5, // 结束游戏，给玩家做win标记。

	NONE_WIN = 0, // 没人赢
	SEEKER_WIN, // 猎人赢
	HIDER_WIN, // 逃生者赢
};

struct CScoreLoadBestTimeResult;
class CGameControllerDDRace : public IGameController
{
public:
	CGameControllerDDRace(class CGameContext *pGameServer);
	~CGameControllerDDRace();

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;

	// 生成一个包含[0,a-1]范围内的b个不重复随机整数的数组
	std::vector<int> unique_random_numbers(int a, int b)
	{
		// 创建一个空的数组
		std::vector<int> result;
		// 如果b大于a，返回空数组
		if(b > a)
		{
			return result;
		}
		// 创建一个随机数生成器
		std::random_device rd;
		std::mt19937 gen(rd());
		// 创建一个均匀分布
		std::uniform_int_distribution<int> dis(0, a - 1);
		// 循环b次
		for(int i = 0; i < b; i++)
		{
			// 生成一个随机数
			int num = dis(gen);
			// 检查是否已经在数组中
			bool exists = std::find(result.begin(), result.end(), num) != result.end();
			// 如果不存在，添加到数组中
			if(!exists)
			{
				result.push_back(num);
			}
			else
			{
				// 如果存在，重新生成一个随机数
				i--;
			}
		}
		// 返回数组
		return result;
	}
	static void HiddenTeleportPlayerToPosition(CCharacter *pChr, vec2 Pos); // 传送角色到Pos
	void HiddenTeleportPlayerToCheckPoint(CPlayer *pPlayer, int TeleTo); // 传送角色到CP点
	void HiddenStepUpdate(int toStep); // 阶段更新
	void HiddenTick(int nowTick, int endTick, int tickSpeed, int nowStep); // Tick处理
	bool HiddenIsPlayerGameOver(CPlayer *pPlayer); // 判断玩家是否被淘汰
	bool HiddenIsMachine(CPlayer *pPlayer); // 判断玩家是否是假人机器设备

	bool m_HiddenState = false; // Hidden Mode状态
	bool m_KillHammer = false; // 杀人锤子状态
	bool m_HiddenModeCanTurnOn = false; // 是否可开启hidden mode

	struct
	{ // Hidden Mode 模式数据
		int nowStep = STEP_S0; // 当前阶段
		int stepDurationTime = -1; // 阶段持续时间 秒计数
		int stepStartTick = -1; // 阶段开始时的Tick
		int stepEndTick = -1; // 阶段结束时的Tick
		int whoWin = NONE_WIN; // 哪边赢了

		int machineNum = 0; // 机器数量
		int activedMachine = 0; // 已激活机器数量
		int seekerNum = 0; // 猎人数量

		int iS1PlayerNum = 0; // S1时玩家数量
		int lastActiveClientID = 0; // 最后行动的ClientID

		bool canZoom = true; // 可以缩放、旁观、spec等操作
		int deviceNum = 4; // 设备数量

		CPickup *a_pHealthPointerList[MAX_CLIENTS]; // health指南针列表
		bool isCreatedGlobalHealthList = false; // 是否创建了health指南针列表
	} m_Hidden;
	// 重置Hidden Mode各种状态
	void HiddenStateReset()
	{
		m_Hidden.nowStep = STEP_S0;
		m_Hidden.stepDurationTime = -1;
		m_Hidden.stepStartTick = -1;
		m_Hidden.stepEndTick = -1;
		m_Hidden.whoWin = NONE_WIN;

		m_Hidden.machineNum = 0;
		m_Hidden.activedMachine = 0;
		m_Hidden.seekerNum = 0;

		m_Hidden.iS1PlayerNum = 0;
		m_Hidden.lastActiveClientID = 0;
		m_Hidden.canZoom = true;
	}
	// 激活设备假人机器
	void HiddenActiveMachine(CPlayer *pPlayer, CPlayer *pMachine)
	{
		if(m_Hidden.nowStep != STEP_S4)
			return;
		char aBuf[256];
		m_Hidden.activedMachine++;
		m_Hidden.stepEndTick = Server()->Tick() + Server()->TickSpeed() * Config()->m_HiddenStepDurationS4;

		// 机器进度，百分比
		double process = (double)m_Hidden.activedMachine / m_Hidden.machineNum * 100;
		str_format(aBuf, sizeof(aBuf), "%s %.2f%%", Config()->m_HiddenStepDeviceActivatedProgressMSG, process);
		GameServer()->SendBroadcast(aBuf, -1);
		HiddenTeleportPlayerToCheckPoint(pMachine, 242);

		// 添加到最后一次行动
		m_Hidden.lastActiveClientID = pPlayer->GetCID();
		// 增加分数
		if(pPlayer->m_Score.has_value())
			pPlayer->m_Score = pPlayer->m_Score.value() + 1;

		// 击杀MSG提示
		CNetMsg_Sv_KillMsg Msg;
		Msg.m_Killer = pPlayer->GetCID();
		Msg.m_Victim = pMachine->GetCID();
		Msg.m_Weapon = WEAPON_SHOTGUN;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

		// 声音提示
		GameServer()->CreateSound(pPlayer->GetCharacter()->GetPos(), SOUND_CTF_CAPTURE, pPlayer->GetCharacter()->TeamMask());
	}
	// 判断当前地图是否可以启动hidden mode
	bool HiddenModeCanTurnOn()
	{ /* 地图检索判断
			如果地图不是躲猫猫地图则不能开启Hidden Mode
			在step投票环节中，个位数为0的CP点均是玩家传送点，其余为投票判定点
		 */
		bool HasTele200 = !m_TeleOuts[200 - 1].empty();
		bool HasTele201 = !m_TeleOuts[201 - 1].empty();
		bool HasTele202 = !m_TeleOuts[202 - 1].empty();
		bool step1 = HasTele200 && HasTele201 && HasTele202;

		bool HasTele210 = !m_TeleOuts[210 - 1].empty();
		bool HasTele211 = !m_TeleOuts[211 - 1].empty();
		bool HasTele212 = !m_TeleOuts[212 - 1].empty();
		bool HasTele213 = !m_TeleOuts[213 - 1].empty();
		bool HasTele214 = !m_TeleOuts[214 - 1].empty();
		bool step2 = HasTele210 && HasTele211 && HasTele212 && HasTele213 && HasTele214;

		bool HasTele220 = !m_TeleOuts[220 - 1].empty();
		bool HasTele221 = !m_TeleOuts[221 - 1].empty();
		bool HasTele222 = !m_TeleOuts[222 - 1].empty();
		bool HasTele223 = !m_TeleOuts[223 - 1].empty();
		bool HasTele224 = !m_TeleOuts[224 - 1].empty();
		bool step3 = HasTele220 && HasTele221 && HasTele222 && HasTele223 && HasTele224;

		bool HasTele231 = !m_TeleOuts[231 - 1].empty();
		bool HasTele232 = !m_TeleOuts[232 - 1].empty();
		bool HasTele241 = !m_TeleOuts[241 - 1].empty();
		bool HasTele242 = !m_TeleOuts[242 - 1].empty();
		bool step4 = HasTele231 && HasTele232 && HasTele241 && HasTele242;

		bool HasTele251 = !m_TeleOuts[251 - 1].empty();
		bool HasTele252 = !m_TeleOuts[252 - 1].empty();
		bool step5 = HasTele251 && HasTele252;

		if(!step1 ||
			!step2 ||
			!step3 ||
			!step4 ||
			!step5)
		{
			return false;
		}

		return true;
	}
	// 暂停游戏
	void HiddenPauseGame(bool isPause);
	// 开始游戏
	void HiddenStartGame();
	// 创建health指南针
	void HiddenCreateHealthPointer(int clientID = -1);
	// 移除health指南针
	void HiddenRemoveHealthPointer(int clientID = -1);
	// 定义一个可变参数模板函数，用来根据玩家距离telePos的距离，返回玩家选择的telePos编号，默认返回第一个telePos
	template<typename... Args>
	int HiddenGetPlayerVoteByTelePos(CPlayer *pPlayer, Args... args);
};

#endif // GAME_SERVER_GAMEMODES_DDRACE_H