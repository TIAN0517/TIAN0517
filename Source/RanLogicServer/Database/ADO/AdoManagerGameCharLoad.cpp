#include "pch.h"

#include "../../../SigmaCore/gassert.h"
#include "../../../SigmaCore/Log/LogMan.h"
#include "../../../SigmaCore/Util/DateTime.h"
#include "../../../SigmaCore/String/StringFormat.h"

#include "../../../RanLogic/Msg/GLContrlBaseMsg.h"
#include "../../../RanLogic/Msg/GLContrlCharJoinMsg.h"

#include "../../../RanLogic/Character/CharacterSlot/GLCharSlotMan.h"

#include "../../Server/NetServer.h"
#include "../../Msg/AgentMsg.h"

#include "./ADOManager.h"

#include "../../../SigmaCore/DebugInclude.h"

namespace db
{

int AdoManager::GetCharacterInfoForMigration( int nUserNumber, int nChaNum, SCHARDATA2* pChaData2 )
{
	// 사용하지않는다.
	/*
	if ( nUserNumber <= 0 || nChaNum <= 0 )
		return sc::db::DB_ERROR;

	if ( GetCharacterInfoBase( nUserNumber, nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// Skill
	if ( GetCharacterInfoSkill( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// Skill Quick Slot
	if ( GetCharacterInfoSkillSlot( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// Action Quick Slot
	if ( GetCharacterInfoActionSlot( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;	

	// Quest
	if ( GetCharacterInfoQuestForMigration( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// 착용아이템
	if ( GetCharacterInfoPutOnItemsForMigration( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;	

	// 캐릭터 인벤토리
	if ( GetCharacterInfoInvenForMigration( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	if ( GetCharacterInfoCoolTime( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// 걸려있는 SKillFact
	if ( GetCharacterInfoSkillFact( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	if ( GetCharacterInfoHireSummon( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;
	*/
	return sc::db::DB_OK;
}

//! 캐릭터 정보를 가져온다. 캐릭터의 모든정보 (바이너리 포함)
//! \param nUserNum 사용자번호
//! \param nChaNum 캐릭터번호
//! \param pChaData2 캐릭터 데이터를 받을 포인터
int AdoManager::GetCharacterInfo( int nUserNumber, int nChaNum, SCHARDATA2* pChaData2 )
{
	sc::writeLogInfo ( sc::string::format( "Start GetCharacter %1%", nChaNum ) );

    if ( nUserNumber <= 0 || nChaNum <= 0 )
		return sc::db::DB_ERROR;

    if ( GetCharacterInfoBase( nUserNumber, nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;


	if ( m_ServiceProvider == SP_HONGKONG )
		GetLastCharInfo( nUserNumber, nChaNum, pChaData2 );

	// Stat
	if ( GetCharacterInfoStats( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;
	
	// Skill
    if ( GetCharacterInfoSkill( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;

	// Skill Quick Slot
    if ( GetCharacterInfoSkillQSlot( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;
	
	// Action Quick Slot
    if ( GetCharacterInfoActionSlot( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;

	if ( GetCharacterInfoStreakData( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// Quest
    if ( GetCharacterInfoQuestNew( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;

	// 착용아이템
    if ( GetPutonItems( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;	
	
	// 캐릭터 인벤토리
    if ( GetInvenItems( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;
	
    if ( GetCharacterInfoCoolTime( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;
	
	// 걸려있는 SKillFact
    if ( GetCharacterInfoSkillFact( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
        return sc::db::DB_ERROR;

	// 고용 소환수 정보;
	if ( GetCharacterInfoHireSummon( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	// 캐릭터 스킬 슬롯 타입 정보;
	if ( GetCharacterInfoSlots( nChaNum, pChaData2 ) == sc::db::DB_ERROR )
		return sc::db::DB_ERROR;

	sc::writeLogInfo ( sc::string::format( "End GetCharacter %1%", nChaNum ) );
	
	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoBase(int nUserNumber, int nChaNum, SCHARDATA2* pChaData2)
{    
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum",  nChaNum);
    APPEND_IPARAM_INT(AdoGame, "@UserNum", nUserNumber);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoSelectBase"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;
    
    int RowCount = 0;
    do
    {
        int nUserNum = 0; int nSGNum = 0; int nChaTribe = 0; int nChaClass = 0;
        std::string ChaName;

        AdoGame.GetCollect("UserNum",  nUserNum);
        AdoGame.GetCollect("SGNum",    nSGNum);
        AdoGame.GetCollect("ChaName",  ChaName);
        AdoGame.GetCollect("ChaTribe", nChaTribe);
        AdoGame.GetCollect("ChaClass", nChaClass);

        int nChaBright = 0; int nChaLevel = 0; int nChaDex = 0; int nChaIntel = 0; int nChaPower = 0;
                    
        AdoGame.GetCollect("ChaBright", nChaBright);
        AdoGame.GetCollect("ChaLevel",  nChaLevel);
        AdoGame.GetCollect("ChaDex",    nChaDex);
        AdoGame.GetCollect("ChaIntel",  nChaIntel);
        AdoGame.GetCollect("ChaPower",  nChaPower);

        int nChaStrong = 0; int nChaSpirit = 0; int nChaStrength = 0; int nChaStRemain = 0; int nChaAttackP = 0;

        AdoGame.GetCollect("ChaStrong",   nChaStrong);
        AdoGame.GetCollect("ChaSpirit",   nChaSpirit);
        AdoGame.GetCollect("ChaStrength", nChaStrength);
        AdoGame.GetCollect("ChaStRemain", nChaStRemain);
        AdoGame.GetCollect("ChaAttackP",  nChaAttackP);

        int nChaDefenseP = 0; int nChaFightA = 0; int nChaShootA = 0; int nChaSkillPoint = 0; int nChaHP = 0;
        
        AdoGame.GetCollect("ChaDefenseP",   nChaDefenseP);
        AdoGame.GetCollect("ChaFightA",     nChaFightA);
        AdoGame.GetCollect("ChaShootA",     nChaShootA);
        AdoGame.GetCollect("ChaSkillPoint", nChaSkillPoint);
        AdoGame.GetCollect("ChaHP",         nChaHP);
        
        int nChaMP = 0; int nChaSP = 0; int nChaPK = 0; int nChaStartMap = 0; int nChaStartGate = 0;
        
        AdoGame.GetCollect("ChaMP",        nChaMP);
        AdoGame.GetCollect("ChaSP",        nChaSP);
        AdoGame.GetCollect("ChaPK",        nChaPK);
        AdoGame.GetCollect("ChaStartMap",  nChaStartMap);
        AdoGame.GetCollect("ChaStartGate", nChaStartGate);
        
        float fChaPosX = 0; float fChaPosY = 0; float fChaPosZ = 0; LONGLONG llChaMoney = 0; LONGLONG llChaExp = 0;

        AdoGame.GetCollect("ChaPosX",  fChaPosX);
        AdoGame.GetCollect("ChaPosY",  fChaPosY);
        AdoGame.GetCollect("ChaPosZ",  fChaPosZ);
        AdoGame.GetCollect("ChaMoney", llChaMoney);
        AdoGame.GetCollect("ChaExp",   llChaExp);
        
        int nChaSaveMap = 0; float fChaSavePosX = 0; float fChaSavePosY = 0; float fChaSavePosZ = 0; int nChaSchool = 0;
        
        AdoGame.GetCollect("ChaSaveMap",  nChaSaveMap);
        AdoGame.GetCollect("ChaSavePosX", fChaSavePosX);
        AdoGame.GetCollect("ChaSavePosY", fChaSavePosY);
        AdoGame.GetCollect("ChaSavePosZ", fChaSavePosZ);
        AdoGame.GetCollect("ChaSchool",   nChaSchool);
        
        int nChaHair = 0; int nChaFace = 0; int nChaLiving = 0; int nChaInvenLine = 0; int nChaReturnMap  = 0;            
            
        AdoGame.GetCollect("ChaHair", nChaHair);
        AdoGame.GetCollect("ChaFace", nChaFace);
        AdoGame.GetCollect("ChaLiving", nChaLiving);
        AdoGame.GetCollect("ChaInvenLine", nChaInvenLine);
        AdoGame.GetCollect("ChaReturnMap", nChaReturnMap);
        
        float fChaReturnPosX = 0; float fChaReturnPosY = 0; float fChaReturnPosZ = 0; int nGuNum = 0; std::string ChaGuName;

        AdoGame.GetCollect("ChaReturnPosX", fChaReturnPosX);
        AdoGame.GetCollect("ChaReturnPosY", fChaReturnPosY);
        AdoGame.GetCollect("ChaReturnPosZ", fChaReturnPosZ);
        AdoGame.GetCollect("GuNum", nGuNum);
        AdoGame.GetCollect("ChaGuName", ChaGuName);

        WORD nChaSex = 0; int nChaHairColor = 0; LONGLONG llChaReExp = 0;
        WORD nChaSafeTime = 0; int nChaCP = 0, nChaMacroT = 0, nSkillSlotNum = 0, nChaRebornNum = 0;

        AdoGame.GetCollect("ChaSex", nChaSex);
        AdoGame.GetCollect("ChaHairColor", nChaHairColor);
        AdoGame.GetCollect("ChaReExp", llChaReExp);
        AdoGame.GetCollect("ChaSafeTime", nChaSafeTime);
        AdoGame.GetCollect("ChaCP", nChaCP);
		AdoGame.GetCollect("ChaMacroT", nChaMacroT);
		AdoGame.GetCollect("SkillSlotNum", nSkillSlotNum);
		AdoGame.GetCollect("ChaRebornNum", nChaRebornNum);

		DWORD nChaPkScore = 0; DWORD nChaPkDeath = 0; int nTutorial = 0; int nInvenLock = 0; int nLockerLock = 0;
		AdoGame.GetCollect("ChaPkScore", nChaPkScore);
		AdoGame.GetCollect("ChaPkDeath", nChaPkDeath);
		AdoGame.GetCollect("ChaTutorial", nTutorial);
		AdoGame.GetCollect("ChaInvenLock", nInvenLock);
		AdoGame.GetCollect("ChaLockerLock", nLockerLock);


        pChaData2->m_CharDbNum = (DWORD) nChaNum; // 캐릭터번호
        pChaData2->SetUserID((DWORD) nUserNum ); // 사용자번호
        pChaData2->m_ClubDbNum = (DWORD) nGuNum; // 클럽번호

        if (!ChaName.empty())
            pChaData2->SetName(ChaName);

        if (!ChaGuName.empty())
            pChaData2->SetNickName(ChaGuName);

        pChaData2->m_emTribe			= EMTRIBE(nChaTribe);
        pChaData2->m_emClass			= EMCHARCLASS(nChaClass);

        pChaData2->m_wLevel				= (WORD) nChaLevel;           
        pChaData2->m_sStats.wPow		= (WORD) nChaPower;
        pChaData2->m_sStats.wStr		= (WORD) nChaStrong;
        pChaData2->m_sStats.wSta		= (WORD) nChaStrength;
        pChaData2->m_sStats.wSpi		= (WORD) nChaSpirit;

        pChaData2->m_sStats.wDex		= (WORD) nChaDex;
        pChaData2->m_sStats.wInt		= (WORD) nChaIntel;
        pChaData2->m_wStatsPoint		= (WORD) nChaStRemain;
		pChaData2->SetInvenMoney( llChaMoney );
        pChaData2->m_sExperience.lnNow  = llChaExp;

        pChaData2->m_sHP.nNow			= nChaHP;
        pChaData2->m_sMP.nNow			= nChaMP;
		pChaData2->m_sSP.nNow			= nChaSP;
        pChaData2->m_sStartMapID.dwID	= (DWORD) nChaStartMap;
        pChaData2->m_dwStartGate		= (DWORD) nChaStartGate;
        pChaData2->m_vStartPos.x		= (float) fChaPosX;

        pChaData2->m_vStartPos.y		= (float) fChaPosY;
        pChaData2->m_vStartPos.z		= (float) fChaPosZ,	
        pChaData2->m_nBright				= nChaBright;
        pChaData2->m_powerAttack		= nChaAttackP;
        pChaData2->m_powerDefence	= nChaDefenseP;

        pChaData2->m_powerAttribute[SKILL::EMAPPLY_MELEE]		= nChaFightA;
        pChaData2->m_powerAttribute[SKILL::EMAPPLY_RANGE]		= nChaShootA;
        pChaData2->m_dwSkillPoint		= (DWORD) nChaSkillPoint;        
        pChaData2->m_wPK				= (WORD) nChaPK;

        pChaData2->m_sSaveMapID.dwID	= (DWORD) nChaSaveMap;
        pChaData2->m_vSavePos.x			= (float) fChaSavePosX; 
        pChaData2->m_vSavePos.y			= (float) fChaSavePosY; 
        pChaData2->m_vSavePos.z			= (float) fChaSavePosZ;

        pChaData2->m_sLastCallMapID.dwID= (DWORD) nChaReturnMap;
        pChaData2->m_vLastCallPos.x		= (float) fChaReturnPosX; 
        pChaData2->m_vLastCallPos.y		= (float) fChaReturnPosY; 
        pChaData2->m_vLastCallPos.z		= (float) fChaReturnPosZ;

        pChaData2->m_wSchool			= (WORD) nChaSchool;
        

        pChaData2->m_wFace				= (WORD) nChaFace;
        pChaData2->m_wHair				= (WORD) nChaHair;
        pChaData2->m_nLiving			= (int) nChaLiving;
        pChaData2->m_wHairColor         = (WORD) nChaHairColor;
        pChaData2->m_wSex               = (WORD) nChaSex;

        // 인벤토리 라인
        pChaData2->m_wINVENLINE = (WORD) nChaInvenLine; 

        pChaData2->m_lnRestorableExp = llChaReExp;
        pChaData2->m_fCDMSafeTime = (float) nChaSafeTime;

        pChaData2->m_sCP.wNow = (WORD) nChaCP;
		pChaData2->m_fRemainMacroTime = static_cast<float>(nChaMacroT);

		pChaData2->m_wSlotIndex	= (WORD) nSkillSlotNum;
		pChaData2->m_wRebornNum			= (WORD) nChaRebornNum;
		pChaData2->m_dwPKScore	= (DWORD)nChaPkScore;
		pChaData2->m_dwPKDeath  = (DWORD)nChaPkDeath;
		pChaData2->m_bTutorial = static_cast<bool>(nTutorial);
		pChaData2->m_bInventoryLock = static_cast<bool>(nInvenLock);
		pChaData2->m_bLockerLock = static_cast<bool>(nLockerLock);

		
		++RowCount;
    } while (AdoGame.Next());

    if (RowCount == 1)
	{
        return sc::db::DB_OK;
	}
    else
	{
		sc::writeLogError(
			sc::string::format("AdoManager::GetCharacterInfoBase RowCount %1%", RowCount));
        return sc::db::DB_ERROR;
	}
}

int AdoManager::GetVTCharInfo(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    std::string Query(
        sc::string::format(
            "SELECT saveMoney, saveExp, itemCount FROM dbo.ChaInfo WHERE ChaNum=%1%", nChaNum));
    if (!AdoGame.Execute4Cmd(Query))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR; 

    LONGLONG llChaSaveMoney = 0;
    LONGLONG llChaSaveExp = 0;
    int nChaitemCount = 0;

    do
    {    
        // Bind data			
        AdoGame.GetCollect("saveMoney", llChaSaveMoney);
        AdoGame.GetCollect("saveExp", llChaSaveExp);
        AdoGame.GetCollect("itemCount", nChaitemCount);

        pChaData2->m_lVNGainSysMoney        = llChaSaveMoney;
        pChaData2->m_lVNGainSysExp			= llChaSaveExp;
        pChaData2->m_dwVietnamInvenCount	= nChaitemCount;
    } while (AdoGame.Next());

    return sc::db::DB_OK;
}

// 대만 해킹 문제 관련해서 최근 로그아웃 했을때의 정보를 DB에서 읽어옴.
int AdoManager::GetLastCharInfo(int nUserNum, int nChaNum, SCHARDATA2* pChaData2)
{
    {
        sc::db::AdoExt AdoGame(m_GameDBConnString);
        APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

        if (!AdoGame.ExecuteStoredProcedure("dbo.ChaLastInfoSelect"))
            return sc::db::DB_ERROR;

        if (AdoGame.GetEOF())
            return sc::db::DB_ERROR;
        
        // ChaLastInfo Table의 금액 컬럼의 암호화를 위해서 ChaMoney 컬럼의 DataType을 int -> varchar(100)으로 수정한다.
        // LONGLONG   llChaSaveMoney = 0;	
        // SQLINTEGER cbChaSaveMoney = SQL_NTS; 
        do
        {
            // 캐릭터 로그아웃 할때의 ChaMoney
            std::string TempMoney; int nChaLevel = 0;

            AdoGame.GetCollect("ChaMoney", TempMoney);
            AdoGame.GetCollect("ChaLevel", nChaLevel);
            
            // pChaData2->m_lnTempMoney    = llChaSaveMoney;			
            if (!TempMoney.empty())
            {
				pChaData2->SetTempInvenMoney( DECRYPTMONEY(TempMoney.c_str()) );
            }
            else
            {
                sc::writeLogError(
                    sc::string::format(
                        "AdoManager::GetLastCharInfo TempMoney empty UserNum %1% ChaNum %2%",
                        nUserNum, nChaNum));
            }

            pChaData2->m_wTempLevel = (WORD)nChaLevel;
        } while (AdoGame.Next());
    }       

    {        
        sc::db::AdoExt AdoGame2(m_GameDBConnString);
        APPEND_IPARAM_INT(AdoGame2, "@UserNum", nUserNum);

        if (!AdoGame2.ExecuteStoredProcedure("dbo.UserLastInfoSelect"))
            return sc::db::DB_ERROR;

        if (AdoGame2.GetEOF())
            return sc::db::DB_ERROR;

        // UserLastInfo Table의 금액 컬럼의 암호화를 위해서 ChaMoney 컬럼의 DataType을 int -> varchar(100)으로 수정한다.
        //	LONGLONG   llUserSaveMoney = 0;
        //	SQLINTEGER cbUserSaveMoney = SQL_NTS;
        do
        {
            std::string TempStorageMoney;
            AdoGame2.GetCollect("UserMoney", TempStorageMoney);           
            
            if (!TempStorageMoney.empty())
            {
				pChaData2->SetTempStorageMoney( DECRYPTMONEY(TempStorageMoney.c_str()) );
            }
            else
            {
                sc::writeLogError(
                    sc::string::format(
                        "AdoManager::GetLastCharInfo TempStorageMoney empty UserNum %1% ChaNum %2%",
                        nUserNum, nChaNum));
            }
        } while (AdoGame2.Next());
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoStats ( int nChaNum, SCHARDATA2* pChaData2 )
{
	if ( NULL == pChaData2 )
		return sc::db::DB_ERROR;

	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( FALSE == AdoGame.ExecuteStoredProcedure( "dbo.ChaInfoChaStatsSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_ERROR;

	int nChaDex = 0;
	int nChaInt = 0;
	int nChaPow = 0;
	int nChaStr = 0;
	int nChaSpirit = 0;
	int nChaStrength = 0;
	int nChaStatPoint = 0;
	int nChaSkillPoint = 0;

	AdoGame.GetCollect( "ChaDex", nChaDex );
	AdoGame.GetCollect( "ChaIntel", nChaInt );
	AdoGame.GetCollect( "ChaPower", nChaPow );
	AdoGame.GetCollect( "ChaStrong", nChaStr );
	AdoGame.GetCollect( "ChaSpirit", nChaSpirit );
	AdoGame.GetCollect( "ChaStrength", nChaStrength );
	AdoGame.GetCollect( "ChaStRemain", nChaStatPoint );
	AdoGame.GetCollect( "ChaSkillPoint", nChaSkillPoint );

	pChaData2->m_sStats.wPow = static_cast< WORD >( nChaPow );
	pChaData2->m_sStats.wStr = static_cast< WORD >( nChaStr );
	pChaData2->m_sStats.wSta = static_cast< WORD >( nChaStrength );
	pChaData2->m_sStats.wSpi = static_cast< WORD >( nChaSpirit );
	pChaData2->m_sStats.wDex = static_cast< WORD >( nChaDex );
	pChaData2->m_sStats.wInt = static_cast< WORD >( nChaInt );
	pChaData2->m_wStatsPoint = static_cast< WORD >( nChaStatPoint );
	pChaData2->m_dwSkillPoint = static_cast< DWORD >( nChaSkillPoint );

	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoSkill(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaSkillsSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;
    
    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaSkills", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETEXPSKILLS_BYBUF(ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoSkillQSlot(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaSkillQSlotSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaSkillSlot", vBuffer);    

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETSKILL_QUICKSLOT(ByteStream);
    }

    return sc::db::DB_OK;
}
int AdoManager::GetCharacterInfoStreakData(int nChaNum, SCHARDATA2* pChaData2)
{    
	sc::db::AdoExt AdoGame(m_GameDBConnString);
	APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

	if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaPKStreak"))
		return sc::db::DB_ERROR;

	if (AdoGame.GetEOF())
		return sc::db::DB_ERROR;

	std::vector<BYTE> vBuffer;
	AdoGame.GetChunk("ChaPKStreak", vBuffer);

	if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
	{
		se::ByteStream ByteStream(vBuffer);
		pChaData2->SETPKSTREAK(ByteStream);
	}

	return sc::db::DB_OK;
}


int AdoManager::GetCharacterInfoActionSlot(int nChaNum, SCHARDATA2* pChaData2)
{    
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaActionSlotSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaActionSlot", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETACTION_QUICKSLOT(ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoQuest(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaQuestSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaQuest", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETQUESTPLAY(ByteStream, true);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoQuestForMigration( int nChaNum, SCHARDATA2* pChaData2 )
{
	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( !AdoGame.ExecuteStoredProcedure( "dbo.ChaInfoChaQuestSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_ERROR;

	std::vector< BYTE > vBuffer;
	AdoGame.GetChunk( "ChaQuest", vBuffer );

	if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
	{
		se::ByteStream ByteStream( vBuffer );
		pChaData2->SETQUESTPLAY_FOR_MIGRATION( ByteStream, true );
	}

	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoQuestNew( int nChaNum, SCHARDATA2* pChaData2 )
{
	// 진행 퀘스트 아이템 가져오기
	std::multimap< DWORD, SINVENITEM_SAVE > mapProcItems;
	ItemSelect( nChaNum, INVEN_QUEST_PROC, mapProcItems );

	// 완료 퀘스트 아이템 가져오기 : 아이템이 있는가? 필요없을 것 같은데
	std::multimap< DWORD, SINVENITEM_SAVE > mapEndItems;
	ItemSelect( nChaNum, INVEN_QUEST_END, mapEndItems );

	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( !AdoGame.ExecuteStoredProcedure( "dbo.ChaInfoChaQuestSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_ERROR;

	std::vector< BYTE > vBuffer;
	AdoGame.GetChunk( "ChaQuest", vBuffer );

	std::vector< sc::MGUID > vecDeleteItems;
	if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
	{
		se::ByteStream ByteStream( vBuffer );
		pChaData2->SETQUESTPLAY_NEW( ByteStream, true, mapProcItems, mapEndItems, vecDeleteItems );
	}

	// 삭제할(퀘스트가 없어진 경우) 퀘스트 아이템 삭제
	size_t ItemSize = vecDeleteItems.size();
	for ( size_t loop = 0; loop < ItemSize; loop++ )
	{
		ItemDelete( vecDeleteItems[loop], db::DB_DELETE, DELETE_QUEST_ITEM_LOAD );
	}

	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoPutOnItems(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaPutOnItemsSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaPutOnItems", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        SETPUTONITEMS_BYBUF(pChaData2->m_PutOnItems, ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoPutOnItemsForMigration( int nChaNum, SCHARDATA2* pChaData2 )
{
	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( !AdoGame.ExecuteStoredProcedure( "dbo.ChaInfoChaPutOnItemsSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_ERROR;

	std::vector< BYTE > vBuffer;
	AdoGame.GetChunk( "ChaPutOnItems", vBuffer );

	if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
	{
		se::ByteStream ByteStream( vBuffer );
		SETPUTONITEMS_BYBUF_FOR_MIGRATION( pChaData2->m_PutOnItems, ByteStream );
	}

	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoInven(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaInvenSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaInven", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETINVENTORY_BYBUF(ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoInvenForMigration( int nChaNum, SCHARDATA2* pChaData2 )
{
	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( !AdoGame.ExecuteStoredProcedure( "dbo.ChaInfoChaInvenSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_ERROR;

	std::vector< BYTE > vBuffer;
	AdoGame.GetChunk( "ChaInven", vBuffer );

	if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
	{
		se::ByteStream ByteStream( vBuffer );
		pChaData2->SETINVENTORY_BYBUF_FOR_MIGRATION( ByteStream );
	}

	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoVTAddInven(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoVTAddInvenSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("VTAddInven", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETVTADDINVENTORY_BYBUF(ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoCoolTime(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaCoolTimeSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaCoolTime", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETITEMCOOLTIME_BYBUF(ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoSkillFact(int nChaNum, SCHARDATA2* pChaData2)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@ChaNum", nChaNum);

    if (!AdoGame.ExecuteStoredProcedure("dbo.ChaInfoChaSkillFactSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("ChaSkillFact", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETSKILLFACT_BYBUF(ByteStream);
    }

    return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoHireSummon(int nChaNum, SCHARDATA2* pChaData2)
{
	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( !AdoGame.ExecuteStoredProcedure( "dbo.sp_ChaHireSummonInfoSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
	{
		return sc::db::DB_OK;
	}

	int iCrowMainID = 0;
	int iCrowSubID = 0;

	AdoGame.GetCollect( "CrowMainID", iCrowMainID );
	AdoGame.GetCollect( "CrowSubID", iCrowSubID );

	pChaData2->m_sBasicSummonID.SetMid( static_cast< WORD >( iCrowMainID ) );
	pChaData2->m_sBasicSummonID.SetSid( static_cast< WORD >( iCrowSubID ) );

	return sc::db::DB_OK;
}

int AdoManager::GetCharacterInfoSlots ( int nChaNum, SCHARDATA2* pChaData2 )
{
	if ( NULL == pChaData2 )
		return sc::db::DB_ERROR;

	// 설정에 따른 슬롯 상태를 먼저 세팅한다;
	for ( UINT i=0; i<EMCHAR_SLOT_DATA_SIZE; ++i )
	{
		// 개방 슬롯이 아닐경우 락을 건다;
		if ( GLCharSlotMan::Instance()->GetOpenSlotCount() <= i )
		{
			pChaData2->m_arrLockSlotState[ i ] = true;
		}
		// 기본 활성화 슬롯일 경우 락을 해제한다;
		else if ( i < GLCharSlotMan::Instance()->GetDefaultActiveSlotCount() )
		{
			pChaData2->m_arrLockSlotState[ i ] = false;
		}
	}

	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@ChaNum", nChaNum );

	if ( FALSE == AdoGame.ExecuteStoredProcedure( "dbo.ChaInfoChaSlotsSelect" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_ERROR;

	do
	{
		int nChaDex = 0;
		int nChaInt = 0;
		int nChaPow = 0;
		int nChaStr = 0;
		int nChaSpirit = 0;
		int nChaStrength = 0;
		int nChaStatPoint = 0;
		int nChaSkillPoint = 0;
		int nLockSlot = 0;
		int nSkillSlotNum = 0;

		AdoGame.GetCollect( "ChaDex", nChaDex );
		AdoGame.GetCollect( "ChaIntel", nChaInt );
		AdoGame.GetCollect( "ChaPower", nChaPow );
		AdoGame.GetCollect( "ChaStrong", nChaStr );
		AdoGame.GetCollect( "ChaSpirit", nChaSpirit );
		AdoGame.GetCollect( "ChaStrength", nChaStrength );
		AdoGame.GetCollect( "ChaStRemain", nChaStatPoint );
		AdoGame.GetCollect( "ChaSkillPoint", nChaSkillPoint );
		AdoGame.GetCollect( "bLockSlot", nLockSlot );
		AdoGame.GetCollect( "SkillSlotNum", nSkillSlotNum );

		if ( nSkillSlotNum < 0 || EMCHAR_SLOT_DATA_SIZE <= nSkillSlotNum )
		{
			continue;
		}

		SCHAR_SLOT_DATA sCharSlotData;

		sCharSlotData.m_sStats.wPow = static_cast< WORD >( nChaPow );
		sCharSlotData.m_sStats.wStr = static_cast< WORD >( nChaStr );
		sCharSlotData.m_sStats.wSta = static_cast< WORD >( nChaStrength );
		sCharSlotData.m_sStats.wSpi = static_cast< WORD >( nChaSpirit );
		sCharSlotData.m_sStats.wDex = static_cast< WORD >( nChaDex );
		sCharSlotData.m_sStats.wInt = static_cast< WORD >( nChaInt );
		sCharSlotData.m_wStatsPoint = static_cast< WORD >( nChaStatPoint );
		sCharSlotData.m_dwSkillPoint = static_cast< DWORD >( nChaSkillPoint );
		
		// 개방 슬롯이 아닐경우 락을 건다;
		if ( GLCharSlotMan::Instance()->GetOpenSlotCount() <= nSkillSlotNum )
		{
			pChaData2->m_arrLockSlotState[ nSkillSlotNum ] = true;
		}
		// 기본 활성화 슬롯일 경우 락을 해제한다;
		else if ( nSkillSlotNum < GLCharSlotMan::Instance()->GetDefaultActiveSlotCount() )
		{
			pChaData2->m_arrLockSlotState[ nSkillSlotNum ] = false;
		}
		// 일반 슬롯일 경우 DB 에 저장된 정보를 따른다;
		else
		{
			pChaData2->m_arrLockSlotState[ nSkillSlotNum ] = ( nLockSlot >= 1 ) ? true : false;
		}

		{
			std::vector< BYTE > vBuffer;
			AdoGame.GetChunk( "ChaSkills", vBuffer );

			if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
			{
				se::ByteStream sByteStream( vBuffer );
				sCharSlotData.SETEXPSKILLS_BYBUF( sByteStream );
			}
		}

		{
			std::vector< BYTE > vBuffer;
			AdoGame.GetChunk( "ChaSkillSlot", vBuffer );

			if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
			{
				se::ByteStream sByteStream( vBuffer );
				sCharSlotData.SETSKILL_QUICKSLOT( sByteStream );
			}
		}

		if ( false == GLCharSlotMan::Instance()->SaveCharSlot( nChaNum, nSkillSlotNum, sCharSlotData ) )
		{
			sc::writeLogError( sc::string::format(
				"[ Character Slot ] [ Failed Save Character Slot, Character : %1%, Slot : %2% ]",
				nChaNum,
				nSkillSlotNum ) );
		}

	} while ( AdoGame.Next() );

	return sc::db::DB_OK;
}

int AdoManager::ReadUserInven(SCHARDATA2* pChaData2)
{
    bool bInven = CheckInven(m_pServer->ServerGroup(), pChaData2->GetUserID());
    if (!bInven)
    {
        MakeUserInven(m_pServer->ServerGroup(), pChaData2->GetUserID());
    }

    {
        sc::db::AdoExt AdoGame(m_GameDBConnString);
        APPEND_IPARAM_INT(AdoGame, "@UserNum", pChaData2->GetUserID());

        if (!AdoGame.ExecuteStoredProcedure("dbo.UserInvenSelect"))
            return sc::db::DB_ERROR;

        if (AdoGame.GetEOF())    
            return sc::db::DB_ERROR;

        LONGLONG   llUserMoney = 0;        
        _variant_t sStorageDate2;
        _variant_t sStorageDate3;
        _variant_t sStorageDate4;

        AdoGame.GetCollect("UserMoney", llUserMoney);
        AdoGame.GetCollect("ChaStorage2", sStorageDate2);
        AdoGame.GetCollect("ChaStorage3", sStorageDate3);
        AdoGame.GetCollect("ChaStorage4", sStorageDate4);            

		pChaData2->SetStorageMoney( llUserMoney );

        // 인벤토리 사용만료기간
        __time64_t Date2 = sc::time::GetTime(sStorageDate2);
        __time64_t Date3 = sc::time::GetTime(sStorageDate3);
        __time64_t Date4 = sc::time::GetTime(sStorageDate4);
        sc::time::AdjustTime(Date2);
        sc::time::AdjustTime(Date3);
        sc::time::AdjustTime(Date4);

        CTime cTemp2(Date2);
        pChaData2->m_tSTORAGE[0] = cTemp2.GetTime();

        CTime cTemp3(Date3);
        pChaData2->m_tSTORAGE[1] = cTemp3.GetTime();
        
        CTime cTemp4(Date4);
        pChaData2->m_tSTORAGE[2] = cTemp4.GetTime();
    }
    
    // image    
    if (ReadUserInven(m_pServer->ServerGroup(), pChaData2->GetUserID(), pChaData2) == sc::db::DB_ERROR)
        return sc::db::DB_ERROR;

    return sc::db::DB_OK;
}

int AdoManager::ReadUserInvenNew( SCHARDATA2* pChaData2, int FirstLoad /*= 0*/ )
{
	bool bInven = CheckInven(m_pServer->ServerGroup(), pChaData2->GetUserID());
	if (!bInven)
	{
		MakeUserInven(m_pServer->ServerGroup(), pChaData2->GetUserID());
	}

	{
		sc::db::AdoExt AdoGame(m_GameDBConnString);
		APPEND_IPARAM_INT(AdoGame, "@UserNum", pChaData2->GetUserID());

		if (!AdoGame.ExecuteStoredProcedure("dbo.UserInvenSelect"))
			return sc::db::DB_ERROR;

		if (AdoGame.GetEOF())    
			return sc::db::DB_ERROR;

		LONGLONG   llUserMoney = 0;        
		_variant_t sStorageDate2;
		_variant_t sStorageDate3;
		_variant_t sStorageDate4;
		int StorageOption = 0;

		AdoGame.GetCollect("UserMoney", llUserMoney);
		AdoGame.GetCollect("ChaStorage2", sStorageDate2);
		AdoGame.GetCollect("ChaStorage3", sStorageDate3);
		AdoGame.GetCollect("ChaStorage4", sStorageDate4);
		AdoGame.GetCollect( "UserInvenOption", StorageOption );

		switch ( m_ServiceProvider )
		{
		case SP_OFFICE_TEST:
			break;
		default:
			{
				if (  1 == FirstLoad && 0 != StorageOption )
				{
					sc::writeLogError( sc::string::format( 
						"critical error AdoManager::ReadUserInvenNew StorageOption usernum %1% chaum %2%", 
						pChaData2->GetUserID(), pChaData2->CharDbNum() ) );
					return sc::db::DB_ERROR;
				}
			}
			break;
		}

		pChaData2->SetStorageMoney( llUserMoney );

		// 인벤토리 사용만료기간
		__time64_t Date2 = sc::time::GetTime(sStorageDate2);
		__time64_t Date3 = sc::time::GetTime(sStorageDate3);
		__time64_t Date4 = sc::time::GetTime(sStorageDate4);
		sc::time::AdjustTime(Date2);
		sc::time::AdjustTime(Date3);
		sc::time::AdjustTime(Date4);

		CTime cTemp2(Date2);
		pChaData2->m_tSTORAGE[0] = cTemp2.GetTime();

		CTime cTemp3(Date3);
		pChaData2->m_tSTORAGE[1] = cTemp3.GetTime();

		CTime cTemp4(Date4);
		pChaData2->m_tSTORAGE[2] = cTemp4.GetTime();
	}

	if ( sc::db::DB_ERROR == GetLockerItems( pChaData2->GetUserID(), pChaData2 ) )
	{
		return sc::db::DB_ERROR;
	}

	return sc::db::DB_OK;
}

// 유저인벤토리가 있는지 체크한다.	
bool AdoManager::CheckInven(int SGNum, DWORD dwUserNum)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_RPARAM_INT(AdoGame);

    APPEND_IPARAM_INT(AdoGame, "@nUserNum", dwUserNum);

    APPEND_OPARAM_INT(AdoGame, "@nReturn");

    return ExecuteStoredProcedureIntReturn("dbo.GetInvenCount", AdoGame, "@nReturn");
}

int AdoManager::MakeUserInven(int SGNum, DWORD dwUserNum)
{
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_RPARAM_INT(AdoGame);

    APPEND_IPARAM_INT(AdoGame, "@nSGNum", SGNum);
    APPEND_IPARAM_INT(AdoGame, "@nUserNum", dwUserNum);

    APPEND_OPARAM_INT(AdoGame, "@nReturn");

    return ExecuteStoredProcedureIntReturn("dbo.MakeUserInven", AdoGame, "@nReturn");
}

// 사용자 인벤토리에서 데이타를 읽어온다.	
int	AdoManager::ReadUserInven(int SGNum, DWORD dwUserNum, SCHARDATA2* pChaData2)
{    
    sc::db::AdoExt AdoGame(m_GameDBConnString);
    APPEND_IPARAM_INT(AdoGame, "@UserNum", dwUserNum);
    
    if (!AdoGame.ExecuteStoredProcedure("dbo.UserInvenInvenSelect"))
        return sc::db::DB_ERROR;

    if (AdoGame.GetEOF())
        return sc::db::DB_ERROR;

    std::vector<BYTE> vBuffer;
    AdoGame.GetChunk("UserInven", vBuffer);

    if (vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE)
    {
        se::ByteStream ByteStream(vBuffer);
        pChaData2->SETSTORAGE_BYBUF(ByteStream);
    }
    return sc::db::DB_OK;
}

int	AdoManager::ReadUserInvenForItemTransfer( int SGNum, DWORD dwUserNum, SCHARDATA2* pChaData2 )
{
	sc::db::AdoExt AdoGame( m_GameDBConnString );
	APPEND_IPARAM_INT( AdoGame, "@UserNum", dwUserNum );

	if ( !AdoGame.ExecuteStoredProcedure( "dbo.UserInvenInvenSelectForItemTransfer" ) )
		return sc::db::DB_ERROR;

	if ( AdoGame.GetEOF() )
		return sc::db::DB_OK;

	std::vector< BYTE > vBuffer;
	AdoGame.GetChunk( "UserInven", vBuffer );

	if ( vBuffer.size() >= sc::db::DB_IMAGE_MIN_SIZE )
	{
		se::ByteStream ByteStream( vBuffer );
		pChaData2->SETSTORAGE_BYBUF( ByteStream );
	}
	return sc::db::DB_OK;
}

// ----------------------------------------------------------------------------
} // namespace db
// ----------------------------------------------------------------------------
