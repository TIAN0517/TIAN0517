
#include "StdAfx.h"

#include "../../../SigmaCore/String/StringFormat.h"

#include "../../../EngineLib/DxTools/d3dfont.h"

#include "../../../EngineLib/DeviceMan.h"
#include "../../../EngineLib/DxTools/DxFontMan.h"
#include "../../../EngineLib/GUInterface/BasicButton.h"
#include "../../../EngineLib/GUInterface/BasicTextBox.h"
#include "../../../EngineLib/GUInterface/UITextControl.h"
#include "../../../EngineLib/GUInterface/GameTextControl.h"
#include "../../../EngineLib/GUInterface/BasicTextButton.h"
#include "../../../EngineLib/GUInterface/BasicLineBox.h"
#include "../../../EngineLib/GUInterface/BasicLineBoxSmart.h"
#include "../../../EngineLib/GUInterface/SwapImage.h"

#include "../../../RanLogicClient/Char/GLCharacter.h"
#include "../../../RanLogicClient/GLGaeaClient.h"
#include "../../../RanLogicClient/Level/GLLevelFileClient.h"
#include "../../../RanLogicClient/CapturedTheField/GLCaptureTheFieldClient.h"
#include "../../../RanLogic/Land/GLMapAxisInfo.h"
#include "../../../RanLogic/Skill/GLSkill.h"
#include "../../../EngineLib/GUInterface/NSGUI.h"

#include "../../InnerInterface.h"

#include "InstanceListBox.h"
#include "./RnCTF.h"

#include "../Skill/SkillImage.h"

#include "../../../RanLogic/GLogicDataMan.h"

#include <boost/function.hpp>
#include <boost/bind.hpp>

// ----------------------------------------------------------------------------
#include "../../../SigmaCore/DebugInclude.h"
// ----------------------------------------------------------------------------

using namespace PVP;
// ----------------------------------------------------------------------------

#define InfoButtonDelayTime 1.0f
#define Button1_DelayTime 3.0f
//#define Button2_DelayTime 1.0f

RnCTFPage::RnCTFPage( GLGaeaClient* pGaeaClient, CInnerInterface* pInterface, EngineDeviceMan* pEngineDevice )
: RnCompetitionObject(pGaeaClient, pInterface, pEngineDevice)
, m_pGaeaClient(pGaeaClient)
, m_pInterface(pInterface)
, m_pText_Title(NULL)
, m_emCTFType(ENUM_CTF_TYPE_NULL)
, m_pJoinButton(NULL)
, m_fJoinButton_Delay(0.0f)
, m_pBuffControl(NULL)
, m_pBuffGapControl(NULL)
, m_pBuffItemControl(NULL)
{
	memset(m_pComp_SchoolImage, 0, sizeof(m_pComp_SchoolImage));
}

RnCTFPage::~RnCTFPage ()
{
}

void RnCTFPage::CreateSubControl ()
{
	RnCompetitionObject::CreateSubControlEx();

	//! CTF Á¤º¸ ´ëÁø ÇÐ¿ø ÀÌ¹ÌÁö
	{
		m_pComp_SchoolImage[SCHOOL_SM] = new CUIControl(m_pEngineDevice);
		m_pComp_SchoolImage[SCHOOL_SM]->CreateSub(this, "COMPETITION_CTF_PAGE_INFO_RACE_SCHOOL0");
		RegisterControl(m_pComp_SchoolImage[SCHOOL_SM]);

		m_pComp_SchoolImage[SCHOOL_HA] = new CUIControl(m_pEngineDevice);
		m_pComp_SchoolImage[SCHOOL_HA]->CreateSub(this, "COMPETITION_CTF_PAGE_INFO_RACE_SCHOOL1");
		RegisterControl(m_pComp_SchoolImage[SCHOOL_HA]);

		m_pComp_SchoolImage[SCHOOL_BH] = new CUIControl(m_pEngineDevice);
		m_pComp_SchoolImage[SCHOOL_BH]->CreateSub(this, "COMPETITION_CTF_PAGE_INFO_RACE_SCHOOL2");
		RegisterControl(m_pComp_SchoolImage[SCHOOL_BH]);
	}

	//! ÅØ½ºÆ®
	{
		CD3DFontPar* pFont = m_pEngineDevice->LoadFont ( _DEFAULT_FONT, 9, _DEFAULT_FONT_FLAG );

		const int nAlignLeftTop    = TEXT_ALIGN_LEFT | TEXT_ALIGN_TOP;	
		const int nAlignLeftCenter = TEXT_ALIGN_LEFT | TEXT_ALIGN_CENTER_Y;	
		const int nAlignCenter	   = TEXT_ALIGN_CENTER_X | TEXT_ALIGN_CENTER_Y;	

		SetNextWar(ID2GAMEWORD("RNCOMPETITION_NEXT_WAR"));
		SetRankText(ID2GAMEWORD("RNCOMPETITION_RANK"));
		SetInstanceListText(ID2GAMEWORD("RNCOMPETITION_INSTANCE_LIST"));
		SetObjectText(ID2GAMEWORD("RNCOMPETITION_OBJECT"));
		SetMoreInfoText(ID2GAMEWORD("RNCOMPETITION_MORE_INFO"));
		SetRewardText(ID2GAMEWORD("RNCOMPETITION_REWARD"));
        RnCompetitionObject::_SetEnableInfoButton(true);
        RnCompetitionObject::_SetEnableButton1(true);

	}

	//! Âü¿©/Ãë¼Ò ¹öÆ°
	{
		m_pJoinButton = new CBasicTextButton(m_pEngineDevice);
		m_pJoinButton->CreateSub(this, "BASIC_TEXT_BUTTON22", UI_FLAG_XSIZE, CTF_BATTLE_BUTTON);
		m_pJoinButton->CreateBaseButton( "RNCOMPETITION_CTF_PAGE_BATTLE_BUTTON", CBasicTextButton::SIZE22, 
			CBasicButton::CLICK_FLIP, ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 0) );
		RegisterControl( m_pJoinButton );
	}
	m_pButton1->SetVisibleSingle(FALSE);
	
	m_pInstanceListBox->AddItem(ID2GAMEINTEXT("MAP_222_0"));
	m_pInstanceListBox->SetCurSel(0);
	

	SetVisibleSingleContribution(FALSE);
	SetVisibleSingleExp(FALSE);
	SetVisibleSingleMoney(FALSE);
	SetVisibleSingleItem(FALSE);
	SetVisibleSingleBuff(FALSE);

	//! bjju.capturethefield
	// ÇöÀç ¹ßÀü¼Ò Á¡·ÉÀü¹Û¿¡ Á¸ÀçÇÏÁö ¾Ê´Â´Ù.
	m_emCTFType = PVP::ENUM_CTF_TYPE_POWERSTATION;


	m_pBuffControl = new CUIControl(m_pEngineDevice);
	m_pBuffControl->CreateSub( this, "COMPETITION_CTF_PAGE_HISTORY_REWARDBUFF" );
	RegisterControl( m_pBuffControl );

	m_pBuffGapControl = new CUIControl(m_pEngineDevice);
	m_pBuffGapControl->CreateSub( this, "COMPETITION_CTF_PAGE_HISTORY_REWARDBUFF_GAP" );
	RegisterControl( m_pBuffGapControl );

	m_pBuffItemControl = new CUIControl(m_pEngineDevice);
	m_pBuffItemControl->CreateSub( this, "COMPETITION_CTF_PAGE_HISTORY_REWARDBUFF_ITEM" );
	RegisterControl( m_pBuffItemControl );
}

void RnCTFPage::Update( int x, int y, BYTE LB, BYTE MB, BYTE RB, int nScroll, float fElapsedTime, BOOL bFirstControl )
{
	RnCompetitionObject::Update ( x, y, LB, MB, RB, nScroll, fElapsedTime, bFirstControl );
	BattleUpdate();
	HistoryUpdate();

	if ( m_fJoinButton_Delay > 0.0f )
	{
		m_fJoinButton_Delay -= fElapsedTime;
		if ( m_fJoinButton_Delay < 0.0f )
		{
			m_fJoinButton_Delay = 0.0f;
		}
	}
}

void RnCTFPage::Update()
{
	UpdateInfo ();
	UpdateBattle ();
	UpdateHistory ();
}

void RnCTFPage::TranslateUIMessage ( UIGUID ControlID, DWORD dwMsg )
{
	if ( ControlID >= CTF_HISTORY_BUFF_BEGIN )
	{
		if ( CHECK_MOUSE_IN ( dwMsg ) )
		{
			GLCaptureTheFieldClient* pCTFClient = m_pGaeaClient->GetCaptureTheField();
			GLCaptureTheFieldClient::CTFPREVSTATE& sState = pCTFClient->GetPrevState( m_emCTFType );

			NSGUI::SetCharMoveBlock();

			UINT nBuffIndex = (UINT)(ControlID - CTF_HISTORY_BUFF_BEGIN);
			CTF_REWARD_BUFF& sRewardBuff = sState.vecREWARDBUFF[nBuffIndex];

			GLCharacter* pCharacter = m_pGaeaClient->GetCharacter();

			SNATIVEID sNativeID ( sRewardBuff.dwID );
			PGLSKILL pSkill = GLSkillMan::GetInstance().GetData ( sNativeID );

			if ( pCharacter && pSkill )
			{	
				CString strName;
				strName = pSkill->GetName();

				CString strDesc;
				pSkill->GetDescFormatByLevel( strDesc, sRewardBuff.wLevel, GLSKILL::DESC_FORMAT_IMPACT | GLSKILL::DESC_FORMAT_SPEC );

				m_pInterface->BEGIN_COMMON_LINEINFO_MULTI( TEXT_ALIGN_LEFT );
				m_pInterface->ADD_COMMON_LINEINFO_MULTI( strName, NS_UITEXTCOLOR::GREENYELLOW );
				m_pInterface->ADD_COMMON_LINEINFO_MULTI( strDesc, NS_UITEXTCOLOR::WHITE );
				m_pInterface->END_COMMON_LINEINFO_MULTI();
			}
		}
	}

	RnCompetitionObject::TranslateUIMessage ( ControlID, dwMsg );

	switch( ControlID )
	{
	case CTF_BATTLE_BUTTON:
		{
			if( CHECK_MOUSEIN_LBUPLIKE ( dwMsg ) )
			{
				// ½ÅÃ»¹öÆ°ÀÇ ¹öÆ° µô·¹ÀÌ°¡ ³²¾Æ ÀÖ´Ù¸é
				if ( m_fJoinButton_Delay > 0.0f )
				{
					m_pInterface->PrintMsgText(NS_UITEXTCOLOR::RED, ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT",10));
					break;
				}

				m_fJoinButton_Delay = Button1_DelayTime;  // ½ÅÃ» µô·¹ÀÌ 3ÃÊ;

				if ( m_fnCallbackButton.empty() )
				{
					break;
				}

				m_fnCallbackButton();
			}
		}
		break;

	case RnCompetitionObject::INFO_BUTTON:
		{
			if( CHECK_MOUSEIN_LBUPLIKE ( dwMsg ) )
			{
				m_pInterface->CompetitionTopRankingSetTextTime( m_strTimeText.c_str() );
				m_pInterface->CompetitionTopRankingOpen( m_emCTFType );
			}
			break;
		}
	}
}

void RnCTFPage::BeginSubControl()
{
	Update();
}

void RnCTFPage::EndSubControl()
{
	m_pInterface->CompetitionTopRankingClose();
}

void RnCTFPage::UpdateInfo ()
{
	GLCaptureTheFieldClient* pCTFClient = m_pGaeaClient->GetCaptureTheField();
	GLCaptureTheFieldClient::CTFGLOBALSTATE& sState = pCTFClient->GetGlobalState( m_emCTFType );

	if ( sState.IsNULL() )
	{
		return;
	}

	//! ¸ÊÀÌ¸§°ú ÀÌ¹ÌÁö Á¤º¸¸¦ ±¸ÇÑ´Ù.
	GLLevelFileClient* pLevelFile = sState.spPVPMAPLEVELDATA.get();
	if ( pLevelFile )
	{
		m_pMap->SetImageTexture( pLevelFile->GetMapAxisInfo().GetMinMapTex() );
		SetInstanceName(m_pGaeaClient->GetMapName ( pLevelFile->GetBaseMapID() ));
	}

	//! Á¤º¸ ÅØ½ºÆ®¸¦ ³Ö´Â´Ù.
	{
		const UIRECT& rcParentPos = GetGlobalPos();
		const UIRECT& rcTextPos   = m_pMoreInfoInfoTextBox->GetGlobalPos();

		CD3DFontPar* pFont = m_pMoreInfoInfoTextBox->GetFont();
		GASSERT(pFont);

		SIZE szSpace;
		pFont->GetTextExtent(" ", szSpace);

		int nPart = 0;

		m_pMoreInfoInfoTextBox->ClearText();

		// ¸ñÇ¥
		{
			m_pObjectTextInfoBox->ClearText();
			std::string strCaption = sc::string::format( ID2GAMEINTEXT("RNCOMPETITION_CTF_OBJECT_INFO"), 3 );
			nPart = m_pObjectTextInfoBox->AddTextWithCRLF( strCaption.c_str(), NS_UITEXTCOLOR::WHITE );
		}

		// ´ëÁø
		{
			for ( UINT i=0; i<SCHOOL_NUM; ++i )
			{
				m_pComp_SchoolImage[i]->SetVisibleSingle(FALSE);
			}
			m_pMoreInfoInfoTextBox->ClearText();
			std::string strCaption = ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_TEXT", 2) + std::string(" ");
			nPart = m_pMoreInfoInfoTextBox->AddTextWithCRLF(strCaption.c_str(), NS_UITEXTCOLOR::WHITE );

			//! ÀÓ½Ã·Î ¸ðµçÇÐ¿øÀÌ °æÀïÇÑ´Ù.
			UINT nCompetitionFlag = (EMTEAM_SM | EMTEAM_HA | EMTEAM_BH);

			if ( nCompetitionFlag & EMTEAM_SM )
			{
				const CBasicTextBox::STEXTPART& sPart = m_pMoreInfoInfoTextBox->GetTextPart( nPart );
				float fLocalX = sPart.m_rcPart.right - rcParentPos.left;
				float fLocalY = sPart.m_rcPart.top   - rcParentPos.top;

				m_pComp_SchoolImage[SCHOOL_SM]->SetVisibleSingle(TRUE);
				m_pComp_SchoolImage[SCHOOL_SM]->SetLocalPos( D3DXVECTOR2(fLocalX, fLocalY) );
				m_pComp_SchoolImage[SCHOOL_SM]->SetGlobalPos( D3DXVECTOR2(sPart.m_rcPart.right, sPart.m_rcPart.top) );

				const UIRECT& rcSize = m_pComp_SchoolImage[SCHOOL_SM]->GetLocalPos();

				//! ÀÌ¹ÌÁöÀÇ »çÀÌÁî ¸¸Å­ °ø¹éÀ¸·Î Ã¤¿î´Ù
				std::string strText = std::string((UINT) ceil( rcSize.sizeX/(float)szSpace.cx ), ' ')  
					+ std::string(" ") + std::string(ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_TEXT", 3)) + std::string(" ");

				nPart = m_pMoreInfoInfoTextBox->AddString(nPart, strText.c_str(), NS_UITEXTCOLOR::WHITE );
			}

			if ( nCompetitionFlag & EMTEAM_HA )
			{
				const CBasicTextBox::STEXTPART& sPart = m_pMoreInfoInfoTextBox->GetTextPart( nPart );
				float fLocalX = sPart.m_rcPart.right - rcParentPos.left;
				float fLocalY = sPart.m_rcPart.top   - rcParentPos.top;

				m_pComp_SchoolImage[SCHOOL_HA]->SetVisibleSingle(TRUE);
				m_pComp_SchoolImage[SCHOOL_HA]->SetLocalPos( D3DXVECTOR2(fLocalX, fLocalY) );
				m_pComp_SchoolImage[SCHOOL_HA]->SetGlobalPos( D3DXVECTOR2(sPart.m_rcPart.right, sPart.m_rcPart.top) );

				const UIRECT& rcSize = m_pComp_SchoolImage[SCHOOL_HA]->GetLocalPos();

				//! ÀÌ¹ÌÁöÀÇ »çÀÌÁî ¸¸Å­ °ø¹éÀ¸·Î Ã¤¿î´Ù
				std::string strText = std::string((UINT) ceil( rcSize.sizeX/(float)szSpace.cx ), ' ')  
					+ std::string(" ") + std::string(ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_TEXT", 3)) + std::string(" ");

				nPart = m_pMoreInfoInfoTextBox->AddString(nPart, strText.c_str(), NS_UITEXTCOLOR::WHITE );
			}

			if ( nCompetitionFlag & EMTEAM_BH )
			{
				const CBasicTextBox::STEXTPART& sPart = m_pMoreInfoInfoTextBox->GetTextPart( nPart );
				float fLocalX = sPart.m_rcPart.right - rcParentPos.left;
				float fLocalY = sPart.m_rcPart.top   - rcParentPos.top;

				m_pComp_SchoolImage[SCHOOL_BH]->SetVisibleSingle(TRUE);
				m_pComp_SchoolImage[SCHOOL_BH]->SetLocalPos( D3DXVECTOR2(fLocalX, fLocalY) );
				m_pComp_SchoolImage[SCHOOL_BH]->SetGlobalPos( D3DXVECTOR2(sPart.m_rcPart.right, sPart.m_rcPart.top) );
			}
		}

		// ÁøÇàÀÎ¿ø
		{
			UINT nPlayersOfSchool = sState.nMAXPLAYERS;

			std::string strCaption = ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_TEXT", 4) + std::string(" ");
			nPart = m_pMoreInfoInfoTextBox->AddTextWithCRLF(strCaption.c_str(), NS_UITEXTCOLOR::WHITE );

			m_pMoreInfoInfoTextBox->AddString(nPart, sc::string::format(ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_PLAYERS", 0), nPlayersOfSchool).c_str(), NS_UITEXTCOLOR::WHITE );
		}

		// Âü°¡·¹º§
		{
			UINT nLevelLimit = static_cast<UINT>(sState.wLEVELLIMIT);

			std::string strCaption = ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_TEXT", 5) + std::string(" ");
			nPart = m_pMoreInfoInfoTextBox->AddTextWithCRLF(strCaption.c_str(), NS_UITEXTCOLOR::WHITE );

			m_pMoreInfoInfoTextBox->AddString(nPart, sc::string::format(ID2GAMEWORD("COMPETITION_CTF_BATTLE_INFO_MAXLEVEL", 0), nLevelLimit).c_str(), NS_UITEXTCOLOR::WHITE );
		}

		GLCharacter * pchar =  m_pGaeaClient->GetCharacter();

		if( !pchar)
			return;

		// * %s È¸ ¿¬¼Ó Á¡·ÉÇÐ¿øÀÔ´Ï´Ù. (´ÙÀ½ Á¡·ÉÀü Âü¿©°¡´É)
		bool bConsecutively = false;
		if(sState.bConsecutivelyCapturel == FALSE)
		{
			if(sState.emSTATE == CTF_STATE_START)
			{
				GLCaptureTheFieldClient::CTFPREVSTATE& sPrevState = pCTFClient->GetPrevState( m_emCTFType );

				if (! sPrevState.IsNULL() )
				{
					if(sPrevState.emAUTHSCHOOL == pchar->GetCharSchool() 
						&& sState.bConsecutivelyCapturel == FALSE
						&& sState.bisConsecutively == FALSE)
					{
						bConsecutively = true;	
					}
				}
			}
			else if( sState.emAUTHSCHOOL != SCHOOL_NONE )
			{
				if (! sState.IsNULL() )
				{
					if(sState.emAUTHSCHOOL == pchar->GetCharSchool() 
						&& sState.bConsecutivelyCapturel == FALSE
						&& sState.bisConsecutively == FALSE)
					{
						bConsecutively = true;
					}
				}
			}

		}

		if(bConsecutively == true)
		{
			UINT nAuthLimit = static_cast<UINT>(sState.dwConsecutivelyNumber);
			std::string strCaption = sc::string::format(ID2GAMEINTEXT("CAPTURETHEFIELD_CONSECUTIVELY", 0) , nAuthLimit);
			nPart = m_pMoreInfoInfoTextBox->AddTextWithCRLF(strCaption.c_str(), NS_UITEXTCOLOR::RED );
		}


	}

	//! º¸»ó ÅØ½ºÆ®¸¦ ³Ö´Â´Ù.
	{
		int nPart = 0;

		m_pRewardInfoTextBox->ClearText();

		//! ÀÓ½Ã·Î º¸»óÀ» ¼³Á¤
		UINT nAwardFlag[] = { 
			(ENUM_REWARD_TYPE_CONTRIBUTION_PLAYER | ENUM_REWARD_TYPE_BUFF_PLAYER),
			(ENUM_REWARD_TYPE_CONTRIBUTION_PLAYER)
		};

		std::string strAwardCaption[] = {
			ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TEXT", 1),
			ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TEXT", 2)
		};

		UINT nMax = sizeof(nAwardFlag)/sizeof(UINT);

		for ( UINT i=0; i<nMax; ++i )
		{
			std::string strCaption = strAwardCaption[i] + std::string(" ");
			nPart = m_pRewardInfoTextBox->AddTextWithCRLF(strCaption.c_str(), NS_UITEXTCOLOR::GREENYELLOW );

			std::string strAward;
			UINT nFlag = nAwardFlag[i];

			if ( nFlag == 0 )
			{
				strAward = ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TYPE", 0);
				m_pRewardInfoTextBox->AddString(nPart, strAward.c_str(), NS_UITEXTCOLOR::GREENYELLOW );
				continue;
			}

			if ( nFlag&ENUM_REWARD_TYPE_CONTRIBUTION_PLAYER )
			{
				if ( !strAward.empty() )
				{
					strAward += ", ";
				}

				strAward += ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TYPE", 1);
			}

			if ( nFlag&ENUM_REWARD_TYPE_CONTRIBUTION_SCHOOL )
			{
				if ( !strAward.empty() )
				{
					strAward += ", ";
				}

				strAward += ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TYPE", 2);
			}

			if ( nFlag&ENUM_REWARD_TYPE_BUFF_PLAYER )
			{
				if ( !strAward.empty() )
				{
					strAward += ", ";
				}

				//! ÀÓ½Ã·Î 4°³
				strAward += sc::string::format(ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TYPE", 3), 4);
			}

			if ( nFlag&ENUM_REWARD_TYPE_BUFF_SCHOOL )
			{
				if ( !strAward.empty() )
				{
					strAward += ", ";
				}

				//! ÀÓ½Ã·Î 4°³
				strAward += sc::string::format(ID2GAMEWORD("COMPETITION_CTF_BATTLE_AWARD_TYPE", 4), 4);
			}

			if( !strAward.empty() )
			{
				m_pRewardInfoTextBox->AddString(nPart, strAward.c_str(), NS_UITEXTCOLOR::GREENYELLOW);
			}
		}
	}
}

void RnCTFPage::UpdateBattle ()
{
	SetCTFType( m_emCTFType );
	BattleUpdate();
}

void RnCTFPage::UpdateHistory ()
{
	SetCTFType( m_emCTFType );
	HistoryUpdate();
}

void RnCTFPage::BattleUpdate()
{
	GLCaptureTheFieldClient* pCTFClient = m_pGaeaClient->GetCaptureTheField();
	GLCaptureTheFieldClient::CTFGLOBALSTATE& sState = pCTFClient->GetGlobalState( m_emCTFType );

	if ( sState.IsNULL() )
	{
		return;
	}

	GLCharacter* pchar = m_pGaeaClient->GetCharacter();
	if(!pchar)
		return;

	// ÁøÇà»óÅÂ
	{
		switch( sState.emSTATE )
		{
			// ³²Àº½Ã°£À» Ç¥½Ã
		case CTF_STATE_STOP:
		case CTF_STATE_READY:
			{
				CTime tCurTime( m_pGaeaClient->GetCurrentTime() );

				int nServerMinute = (int) tCurTime.GetMinute();
				static int nServerMinuteBACK(0);
				if ( nServerMinute != nServerMinuteBACK )
				{
					nServerMinuteBACK = nServerMinute;

					//	¼­¹ö Å¸ÀÓÀ¸·Î º¯È¯( TimeZone °è»ê )
					m_pGaeaClient->GetConvertServerTime( tCurTime );

					CTimeSpan sTimeLeft( sState.tSTARTTIME - tCurTime.GetTime() );

					int nTotalMinutes = (int)(60 * sTimeLeft.GetTotalSeconds() / 3600);
					UINT nTimeHour = static_cast<UINT>( nTotalMinutes / 60 );
					UINT nTimeMinutes = static_cast<UINT>( nTotalMinutes % 60 );
					
					//½Ã°£ Ãâ·Â ¿¹¿ÜÃ³¸®(¼­¹ö¿¡¼­ ½Ã°£À» ´Ê°Ô ¹Þ¾Æ ¿Ã ½Ã ¾²·¹±â°ª ¹æÁö¿ë)
					nTimeHour		= ( nTimeHour>23? 0 : nTimeHour);
					nTimeMinutes	= ( nTimeMinutes>59? 0 : nTimeMinutes);

					std::string strFormat = sc::string::format(ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 8), nTimeHour, nTimeMinutes);
					m_pNextWarTimeTextBox->SetOneLineText( strFormat.c_str(), NS_UITEXTCOLOR::WHITE );
				}
			}
			break;

			// ÁøÇàÁß 
		case CTF_STATE_START:
		case CTF_STATE_REWARD:
		case CTF_STATE_DONE:
			{
				m_pNextWarTimeTextBox->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 7), NS_UITEXTCOLOR::WHITE );
			}
			break;
		};
	}

	// ½ÅÃ»»óÅÂ
	{
		if ( sState.IsJOINDENIED(pchar) )
		{
			m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 3), NS_UITEXTCOLOR::RED );
			goto _STATUS_END;
		}

		switch( sState.emSTATE )
		{
		case CTF_STATE_REWARD:
		case CTF_STATE_DONE:
		case CTF_STATE_STOP:
			{
				m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 3), NS_UITEXTCOLOR::RED );
			}
			break;

		case CTF_STATE_READY:
		case CTF_STATE_START:
			{
				if ( pCTFClient->GetProgressCTF() == m_emCTFType )
				{
					m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 4), NS_UITEXTCOLOR::WHITE );
				}
				else
				{
					if ( sState.bWAITJOIN )
					{
						m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 5), NS_UITEXTCOLOR::ORANGE );
					}
					else
					{
						m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 6), NS_UITEXTCOLOR::GREENYELLOW );
					}
				}
			}
			break;
		};
	}

_STATUS_END:

	//¹öÆ°
	{
		std::string strButtonLabel("");
		bool bEnable = true;

		if ( sState.IsJOINDENIED(pchar) )
		{
			EnableJoinButton( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 0), false );
			m_fnCallbackButton = boost::bind( &RnCTFPage::ReqJoin, this );
			goto _BUTTON_END;
		}

		// ÀÌ¹Ì Âü¿©ÁßÀÌ¶ó¸é
		if ( pCTFClient->GetProgressCTF() == m_emCTFType )
		{
			strButtonLabel     = ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 2);
			m_fnCallbackButton = boost::bind( &RnCTFPage::ReqExit, this );
		}
		else
		{
			if ( sState.bWAITJOIN )
			{
				strButtonLabel     = ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 1);
				m_fnCallbackButton = boost::bind( &RnCTFPage::ReqCancel, this );
			}
			else
			{
				strButtonLabel     = ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 0);
				m_fnCallbackButton = boost::bind( &RnCTFPage::ReqJoin, this );
			}
		}

		if(sState.bConsecutivelyCapturel == FALSE)
		{
			if(sState.emSTATE == CTF_STATE_START)
			{
				GLCaptureTheFieldClient::CTFPREVSTATE& sPrevState = pCTFClient->GetPrevState( m_emCTFType );

				if (! sPrevState.IsNULL() )
				{
					if(sPrevState.emAUTHSCHOOL == pchar->GetCharSchool() 
						&& sState.bConsecutivelyCapturel == FALSE
						&& sState.bisConsecutively == FALSE)
					{
						m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 3), NS_UITEXTCOLOR::RED );
						bEnable = false;	
					}
				}
			}
			else if( sState.emAUTHSCHOOL != SCHOOL_NONE )
			{
				if (! sState.IsNULL() )
				{
					if(sState.emAUTHSCHOOL == pchar->GetCharSchool() 
						&& sState.bConsecutivelyCapturel == FALSE
						&& sState.bisConsecutively == FALSE)
					{
						m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 3), NS_UITEXTCOLOR::RED );
						bEnable = false;	
					}
				}
			}

		}


		if(bEnable == true)
		{
			if ( sState.IsCANNOTJOIN(pchar->GetCharSchool()))
			{
				m_pStatus->SetOneLineText( ID2GAMEWORD("COMPETITION_CTF_BATTLE_TEXT", 3), NS_UITEXTCOLOR::RED );
				bEnable = false;

			}
			else
			{
				bEnable = true;
			}
		}
		EnableJoinButton( strButtonLabel, bEnable );
	}

_BUTTON_END:

	return;
}

void RnCTFPage::EnableJoinButton( const std::string& strLabel, const bool bEnable )
{
	if ( bEnable )
	{
		m_pJoinButton->SetOneLineText( strLabel.c_str() );
		m_pJoinButton->SetDiffuse( NS_UITEXTCOLOR::WHITE );
		m_pJoinButton->SetDiffuseAlpha( 0xFF );
		m_pJoinButton->SetNoMessage( FALSE );
	}
	else
	{
		m_pJoinButton->SetOneLineText( strLabel.c_str() );
		m_pJoinButton->SetDiffuse( NS_UITEXTCOLOR::DIMGREY );
		m_pJoinButton->SetDiffuseAlpha( 0xDD );
		m_pJoinButton->SetNoMessage( TRUE );
	}
}

void RnCTFPage::ReqJoin()
{
	if ( m_pGaeaClient )
	{
		if (m_pGaeaClient->GetCharacter()->IsDie())
			return; 
		m_pGaeaClient->ReqCaptureTheFieldJoin( m_emCTFType );
	}
}

void RnCTFPage::ReqCancel()
{
	if ( m_pGaeaClient )
	{
		m_pGaeaClient->ReqCaptureTheFieldCancelJoin( m_emCTFType );
	}
}

void RnCTFPage::ReqExit()
{
	if ( m_pGaeaClient )
	{
		m_pGaeaClient->ReqCaptureTheFieldQuit();
	}
}

CBasicTextBox* RnCTFPage::CreateStaticControl(const char* szControlKeyword, CD3DFontPar* pFont, int nAlign )
{
	CBasicTextBox* pStaticText = new CBasicTextBox(m_pEngineDevice);
	pStaticText->CreateSub ( this, szControlKeyword, UI_FLAG_DEFAULT );
	pStaticText->SetFont ( pFont );
	pStaticText->SetTextAlign ( nAlign );	
	RegisterControl ( pStaticText );

	return pStaticText;
}

void RnCTFPage::HistoryUpdate()
{
	GLCaptureTheFieldClient* pCTFClient = m_pGaeaClient->GetCaptureTheField();
	GLCaptureTheFieldClient::CTFPREVSTATE& sState = pCTFClient->GetPrevState( m_emCTFType );

	// ÁøÇà½Ã°£
	{
		if ( !sState.IsNULL() && sState.tSTARTTIME && sState.tENDTIME )
		{
			CTime cStartTime( sState.tSTARTTIME );
			CTime cEndTime( sState.tENDTIME );

			m_pGaeaClient->GetConvertServerTime( cStartTime );
			m_pGaeaClient->GetConvertServerTime( cEndTime );

			std::string strFormat = sc::string::format("(%02d:%02d~%02d:%02d)", 
				cStartTime.GetHour(), cStartTime.GetMinute(),
				cEndTime.GetHour(), cEndTime.GetMinute()
				);

			m_strTimeText = strFormat;
			//m_pText_Time->SetOneLineText(strFormat.c_str(), NS_UITEXTCOLOR::WHITE);
		}
		else
		{
			std::string strFormat("(--:--~--:--)");
			m_strTimeText = strFormat;
			//m_pText_Time->SetOneLineText("-- : -- ~ -- : --", NS_UITEXTCOLOR::WHITE);
		}
	}
	// 보상버프
	{
		UINT nMaxBuff = (UINT)sState.vecREWARDBUFF.size();

		VEC_SKILLIMAGE_ITER iter = m_vecSkillImage.begin();
		for (; iter!=m_vecSkillImage.end(); ++iter)
		{
			SKILLIMAGE& sSkillImage = (*iter);
			sSkillImage.pImage->SetVisibleSingle( FALSE );
			sSkillImage.pLine->SetVisibleSingle( FALSE );
		}

		if ( nMaxBuff != 0 )
		{

			const UIRECT& rcParant		= GetGlobalPos();
			const UIRECT& rcBase		= m_pBuffControl->GetLocalPos();
			const UIRECT& rcGap			= m_pBuffGapControl->GetLocalPos();
			const UIRECT& rcItem		= m_pBuffItemControl->GetLocalPos();

			float fBeginX = (rcBase.left + rcBase.right)/2.0f - ((rcItem.sizeX*(float)nMaxBuff) + (rcGap.sizeX*(float)(nMaxBuff-1)))/2.0f;
			float fPosX	  = fBeginX;
			float fPosY	  = (rcBase.top + rcBase.bottom)/2.0f - rcItem.sizeY/2.0f;

			VCTF_REWARD_BUFF_ITER RewardIter = sState.vecREWARDBUFF.begin();
			for ( UINT i=0; RewardIter!=sState.vecREWARDBUFF.end(); ++RewardIter, ++i )
			{
				CTF_REWARD_BUFF& sRewardBuff = (*RewardIter);

				CSkillImage*		pSkillImage = NULL;
				CBasicLineBoxSmart* pLineBox	= NULL;

				if ( i >= (UINT)m_vecSkillImage.size() )
				{
					GASSERT(m_ppd3dDevice);
					pSkillImage = new CSkillImage(m_pGaeaClient, m_pEngineDevice);
					pSkillImage->CreateSub ( this, "COMPETITION_CTF_PAGE_HISTORY_REWARDBUFF_ITEM", UI_FLAG_DEFAULT, CTF_HISTORY_BUFF_BEGIN + i );	
					pSkillImage->InitDeviceObjects ( m_pEngineDevice->GetDevice() );
					pSkillImage->RestoreDeviceObjects ( m_pEngineDevice->GetDevice() );
					pSkillImage->SetUseRender ( TRUE );
					RegisterControl ( pSkillImage );

					pLineBox = new CBasicLineBoxSmart(m_pEngineDevice);
					pLineBox->CreateSub( this, "COMPETITION_CTF_PAGE_HISTORY_REWARDBUFF_LINEBOX" );
					pLineBox->CreateSubControl( "COMPETITION_CTF_PAGE_INFO_LINEBOX_TEXINFO_W_BLACK" );
					pLineBox->InitDeviceObjects ( m_pEngineDevice->GetDevice() );
					pLineBox->RestoreDeviceObjects ( m_pEngineDevice->GetDevice() );
					RegisterControl( pLineBox );

					m_vecSkillImage.push_back(SKILLIMAGE(pSkillImage, pLineBox));
				}
				else
				{
					pSkillImage = m_vecSkillImage[i].pImage;
					pLineBox	= m_vecSkillImage[i].pLine;
				}

				pSkillImage->SetSkill( SNATIVEID(sRewardBuff.dwID) );
				pSkillImage->SetLocalPos( D3DXVECTOR2(fPosX, fPosY) );
				pSkillImage->SetGlobalPos( D3DXVECTOR2(rcParant.left + fPosX, rcParant.top + fPosY) );
				pSkillImage->SetVisibleSingle( TRUE );

				pLineBox->SetLocalPos( D3DXVECTOR2(fPosX-1.0f, fPosY-1.0f) );
				pLineBox->SetGlobalPos( D3DXVECTOR2(rcParant.left + fPosX - 1.0f, rcParant.top + fPosY - 1.0f) );
				pLineBox->SetVisibleSingle( TRUE );

				fPosX += (rcItem.sizeX + rcGap.sizeX);
			}
		}
	}
}

void RnCTFPage::SetVisibleSingle( BOOL bVisible )
{
	if(bVisible)
	{
		CUIWindowObject::SetVisibleSingle(bVisible);
		RnCTFPage::Update();
	}
	else
	{
		CUIWindowObject::SetVisibleSingle(bVisible);
	}
}
