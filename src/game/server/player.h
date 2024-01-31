/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/alloc.h>
#include <game/server/save.h>

#include "teeinfo.h"

#include <memory>
#include <optional>

#include "algorithm"
#include "random"
#include "vector"

class CCharacter;
class CGameContext;
class IServer;
struct CNetObj_PlayerInput;
struct CScorePlayerResult;

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, uint32_t UniqueClientID, int ClientID, int Team);
	~CPlayer();

	void Reset();

	void TryRespawn();
	void Respawn(bool WeakHook = false); // with WeakHook == true the character will be spawned after all calls of Tick from other Players
	CCharacter *ForceSpawn(vec2 Pos); // required for loading savegames
	void SetTeam(int Team, bool DoChatMsg = true);
	int GetTeam() const { return m_Team; }
	int GetCID() const { return m_ClientID; }
	uint32_t GetUniqueCID() const { return m_UniqueClientID; }
	int GetClientVersion() const;
	bool SetTimerType(int TimerType);

	void Tick();
	void PostTick();

	// will be called after all Tick and PostTick calls from other players
	void PostPostTick();
	void Snap(int SnappingClient);
	void FakeSnap();

	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnPredictedEarlyInput(CNetObj_PlayerInput *pNewInput);
	void OnDisconnect();

	void KillCharacter(int Weapon = WEAPON_GAME, bool SendKillMsg = true);
	CCharacter *GetCharacter();
	const CCharacter *GetCharacter() const;

	void SpectatePlayerName(const char *pName);
	// 打乱皮肤数组
	// 例如["default","cow","dragon"]
	// 变为["cow","dragon","default"]
	static std::vector<std::string> ShuffleSkin(std::vector<std::string> arr)
	{
		std::random_device rd; // 创建一个随机数生成器
		std::mt19937 g(rd()); // 创建一个梅森旋转算法引擎
		std::shuffle(arr.begin(), arr.end(), g); // 使用shuffle()函数打乱数组
		return arr; // 返回打乱后的数组
	}

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	int m_TuneZone;
	int m_TuneZoneOld;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags;

	// hidden mode state for the player 躲猫猫模式玩家状态结构体
	struct
	{
		// 在游戏中
		bool m_InGame = false;
		// 是猎人
		bool m_IsSeeker = false;
		// 锁定皮肤
		bool m_IsLockedTeeInfos = false;
		// 被猎人杀害
		bool m_HasBeenKilled = false;
		// 该玩家胜利
		bool m_IsWin = false;
		// 该玩家失败
		bool m_IsLose = false;
		// 首次进入游戏
		bool m_isFirstEnterGame = true;

		// 设备、假人、机器属性

		// 刷新tick
		int endTick = -1;
		// 是假人
		bool m_IsDummyMachine = false;
		// 是占位假人，用以提高排名
		bool m_IsPlaceholder = false;
		// 被警告AFK
		bool m_IsWarnedAFK = false;
	} m_Hidden;
	// 状态重置
	void HiddenStateReset()
	{
		m_Hidden.m_InGame = false;
		m_Hidden.m_IsSeeker = false;
		m_Hidden.m_IsLockedTeeInfos = false;
		m_Hidden.m_HasBeenKilled = false;
		m_Hidden.m_IsWin = false;
		m_Hidden.m_IsLose = false;
		// bool m_isFirstEnterGame = false;
	}

	// used for snapping to just update latency if the scoreboard is active
	int m_aCurLatency[MAX_CLIENTS];

	int m_SentSnaps = 0;

	// used for spectator mode
	int m_SpectatorID;

	bool m_IsReady;

	//
	int m_Vote;
	int m_VotePos;
	//
	int m_LastVoteCall;
	int m_LastVoteTry;
	int m_LastChat;
	int m_LastSetTeam;
	int m_LastSetSpectatorMode;
	int m_LastChangeInfo;
	int m_LastEmote;
	int m_LastEmoteGlobal;
	int m_LastKill;
	int m_aLastCommands[4];
	int m_LastCommandPos;
	int m_LastWhisperTo;
	int m_LastInvited;

	int m_SendVoteIndex;

	CTeeInfo m_TeeInfos;

	int m_DieTick;
	int m_PreviousDieTick;
	std::optional<int> m_Score;
	int m_JoinTick;
	bool m_ForceBalanced;
	int m_LastActionTick;
	int m_TeamChangeTick;

	// network latency calculations
	struct
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	} m_Latency;

private:
	const uint32_t m_UniqueClientID;
	CCharacter *m_pCharacter;
	int m_NumInputs;
	CGameContext *m_pGameServer;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning;
	bool m_WeakHookSpawn;
	int m_ClientID;
	int m_Team;

	int m_Paused;
	int64_t m_ForcePauseTime;
	int64_t m_LastPause;
	bool m_Afk;

	int m_DefEmote;
	int m_OverrideEmote;
	int m_OverrideEmoteReset;
	bool m_Halloween;

public:
	enum
	{
		PAUSE_NONE = 0,
		PAUSE_PAUSED,
		PAUSE_SPEC
	};

	enum
	{
		TIMERTYPE_DEFAULT = -1,
		TIMERTYPE_GAMETIMER = 0,
		TIMERTYPE_BROADCAST,
		TIMERTYPE_GAMETIMER_AND_BROADCAST,
		TIMERTYPE_SIXUP,
		TIMERTYPE_NONE,
	};

	bool m_DND;
	int64_t m_FirstVoteTick;
	char m_aTimeoutCode[64];

	void ProcessPause();
	int Pause(int State, bool Force);
	int ForcePause(int Time);
	int IsPaused() const;

	bool IsPlaying() const;
	int64_t m_Last_KickVote;
	int64_t m_Last_Team;
	int m_ShowOthers;
	bool m_ShowAll;
	vec2 m_ShowDistance;
	bool m_SpecTeam;
	bool m_NinjaJetpack;

	int m_ChatScore;

	bool m_Moderating;

	void UpdatePlaytime();
	void AfkTimer();
	void SetAfk(bool Afk);
	void SetInitialAfk(bool Afk);
	bool IsAfk() const { return m_Afk; }

	int64_t m_LastPlaytime;
	int64_t m_LastEyeEmote;
	int64_t m_LastBroadcast;
	bool m_LastBroadcastImportance;

	CNetObj_PlayerInput *m_pLastTarget;
	bool m_LastTargetInit;

	bool m_EyeEmoteEnabled;
	int m_TimerType;

	int GetDefaultEmote() const;
	void OverrideDefaultEmote(int Emote, int Tick);
	bool CanOverrideDefaultEmote() const;

	bool m_FirstPacket;
	int64_t m_LastSQLQuery;
	void ProcessScoreResult(CScorePlayerResult &Result);
	std::shared_ptr<CScorePlayerResult> m_ScoreQueryResult;
	std::shared_ptr<CScorePlayerResult> m_ScoreFinishResult;
	bool m_NotEligibleForFinish;
	int64_t m_EligibleForFinishCheck;
	bool m_VotedForPractice;
	int m_SwapTargetsClientID; // Client ID of the swap target for the given player
	bool m_BirthdayAnnounced;

	CSaveTee m_LastTeleTee;
};

#endif
