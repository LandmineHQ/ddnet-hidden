/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include "character.h"

#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/teamscore.h>

#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include <game/server/player.h>

static constexpr int gs_PickupPhysSize = 14;

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, int Layer, int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, vec2(0, 0), gs_PickupPhysSize)
{
	m_Core = vec2(0.0f, 0.0f);
	m_Type = Type;
	m_Subtype = SubType;

	m_Layer = Layer;
	m_Number = Number;

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	m_MarkedForDestroy = true;
}

void CPickup::Tick()
{
	CGameControllerDDRace *pController = (CGameControllerDDRace *)GameServer()->m_pController;
	Move();
	// hidden mode
	// HIDDEN_POWERUP_HEALTH
	if(m_Type == POWERUP_HEALTH && m_Subtype == HIDDEN_POWERUP_HEALTH)
	{
		// 拥有该pickup的玩家
		CPlayer *pPlayer = GameServer()->m_apPlayers[this->m_HiddenBindPlayerClient];
		// 该pickup指向的目标玩家/机器
		CPlayer *pTarget = GameServer()->m_apPlayers[0];
		if(!pPlayer || !pPlayer->GetCharacter() || !pTarget || !pTarget->GetCharacter())
			return;
		vec2 vPos = pPlayer->GetCharacter()->m_Pos;
		float min = INFINITY;
		if(!pPlayer->m_Hidden.m_IsSeeker)
		{
			// 逃生者的指南针指向机器
			for(int i = 0; i < pController->m_Hidden.deviceNum; i++)
			{
				auto *pTemp = GameServer()->m_apPlayers[i];
				if(!pTemp || !pTemp->GetCharacter())
					continue;
				float dis = distance(vPos, pTemp->GetCharacter()->m_Pos);
				if(dis < min)
				{
					pTarget = pTemp;
					min = dis;
				}
			}
		}
		else
		{
			// 猎手的指南针指向逃生者
			for(int i = pController->m_Hidden.deviceNum; i < MAX_CLIENTS; i++)
			{
				auto *pTemp = GameServer()->m_apPlayers[i];
				if(!pTemp || !pTemp->GetCharacter())
					continue; // 不能是空指针
				if(pTemp->GetCID() == this->m_HiddenBindPlayerClient)
					continue; // 不能指向自己
				if(pTemp->m_Hidden.m_IsSeeker)
					continue; // 不能指向猎手

				float dis = distance(vPos, pTemp->GetCharacter()->m_Pos);
				if(dis < min)
				{
					pTarget = pTemp;
					min = dis;
				}
			}
		}

		// 根据方向设定health位置，使health位于pPlayers与pTarget之间
		vec2 dir = normalize(pTarget->GetCharacter()->m_Pos - vPos);
		vPos = vPos + dir * 64;
		this->m_Pos = vPos;
	}

	// Check if a player intersected us
	CEntity *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, GetProximityRadius() + ms_CollisionExtraSize, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		auto *pChr = static_cast<CCharacter *>(apEnts[i]);

		if(pChr && pChr->IsAlive())
		{
			if(m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChr->Team()])
				continue;
			bool Sound = false;
			// player picked us up, is someone was hooking us, let them go
			switch(m_Type)
			{
			case POWERUP_HEALTH:
				// hidden mode
				if(m_Subtype == HIDDEN_POWERUP_HEALTH)
					break;
				if(pChr->Freeze())
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
				break;

			case POWERUP_ARMOR:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				for(int j = WEAPON_SHOTGUN; j < NUM_WEAPONS; j++)
				{
					if(pChr->GetWeaponGot(j))
					{
						pChr->SetWeaponGot(j, false);
						pChr->SetWeaponAmmo(j, 0);
						Sound = true;
					}
				}
				pChr->SetNinjaActivationDir(vec2(0, 0));
				pChr->SetNinjaActivationTick(-500);
				pChr->SetNinjaCurrentMoveTime(0);
				if(Sound)
				{
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_SHOTGUN:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_SHOTGUN))
				{
					pChr->SetWeaponGot(WEAPON_SHOTGUN, false);
					pChr->SetWeaponAmmo(WEAPON_SHOTGUN, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() == WEAPON_SHOTGUN)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_GRENADE:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_GRENADE))
				{
					pChr->SetWeaponGot(WEAPON_GRENADE, false);
					pChr->SetWeaponAmmo(WEAPON_GRENADE, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() == WEAPON_GRENADE)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_NINJA:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				pChr->SetNinjaActivationDir(vec2(0, 0));
				pChr->SetNinjaActivationTick(-500);
				pChr->SetNinjaCurrentMoveTime(0);
				break;

			case POWERUP_ARMOR_LASER:
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_LASER))
				{
					pChr->SetWeaponGot(WEAPON_LASER, false);
					pChr->SetWeaponAmmo(WEAPON_LASER, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
				}
				if(pChr->GetActiveWeapon() == WEAPON_LASER)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_WEAPON:

				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || pChr->GetWeaponAmmo(m_Subtype) != -1))
				{
					pChr->GiveWeapon(m_Subtype);

					if(m_Subtype == WEAPON_GRENADE)
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->TeamMask());
					else if(m_Subtype == WEAPON_SHOTGUN)
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());
					else if(m_Subtype == WEAPON_LASER)
						GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());

					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
				}
				break;

			case POWERUP_NINJA:
			{
				// activate ninja on target player
				pChr->GiveNinja();
				break;
			}
			default:
				break;
			};
		}
	}
}

void CPickup::TickPaused()
{
}

void CPickup::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	bool Sixup = Server()->IsSixup(SnappingClient);

	if(SnappingClientVersion < VERSION_DDNET_ENTITY_NETOBJS)
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(SnappingClient);

		if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID != SPEC_FREEVIEW)
			pChar = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->m_SpectatorID);

		int Tick = (Server()->Tick() % Server()->TickSpeed()) % 11;
		if(pChar && pChar->IsAlive() && m_Layer == LAYER_SWITCH && m_Number > 0 && !Switchers()[m_Number].m_aStatus[pChar->Team()] && !Tick)
			return;
	}

	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, Sixup), GetID(), m_Pos, m_Type, m_Subtype, m_Number);
}

void CPickup::Move()
{
	if(Server()->Tick() % (int)(Server()->TickSpeed() * 0.15f) == 0)
	{
		int Flags;
		int index = GameServer()->Collision()->IsMover(m_Pos.x, m_Pos.y, &Flags);
		if(index)
		{
			m_Core = GameServer()->Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}
