#include "../pch.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>


#include <boost/lexical_cast.hpp>

#include "../../SigmaCore/xml/2.5.3/ticpp.h"
#include "../../SigmaCore/Log/LogMan.h"
#include "../../SigmaCore/Util/SystemInfo.h"
#include "../../SigmaCore/Math/SeqUniqueGuid.h"

#include "../../Enginelib/Common/profile.h"
#include "../../Enginelib/DxTools/DebugSet.h"

#include "../../RanLogic/Msg/TraceMsg.h"
#include "../../RanLogic/GLUseFeatures.h"

#include "../Database/DBAction/DbActionUser.h"
#include "../Database/DBAction/DbActionGame.h"
#include "../Database/DBAction/DbActionGamePost.h"
#include "../Database/DBAction/DbActionGameAttendance.h"
#include "../Database/ODBC/s_COdbcManager.h"
#include "../Database/ADO/ADOManager.h"
#include "../Net/NetCachePacket.h"
#include "../Client/NetAgentClientMan.h"
#include "../../RanLogic/Util/GlobalAuthManager.h"
#include "../Util/IpAddressChecker.h"
#include "../PacketObserver/GLPacketObserver.h"
#include "./AgentServerConfigXml.h"
#include "./AgentServer.h"



// ----------------------------------------------------------------------------
#include "../../SigmaCore/DebugInclude.h"
// ----------------------------------------------------------------------------

//#import "EGameEncrypt.dll" raw_interfaces_only, raw_native_types, no_namespace, named_guids

//
//mjeon.ServiceProvider
//
//  moved below objects into the CAgenServer as its member variables.
//

#if defined ( PH_PARAM ) || defined ( VN_PARAM )
	IEGameEncryptPtr    m_pMyRossoEncrypto; //! Terra Decrypt Module
#else
	IMyRossoEncryptPtr    m_pMyRossoEncrypto; //! Terra Decrypt Module
#endif



size_t wrapper_curl_write_callback( void *ptr, size_t size, size_t nmemb, void *userp )
{
	/*
	This function gets called by libcurl as soon as there is data received that needs to be saved.
	The size of the data pointed to by ptr is size multiplied with nmemb,
	it will not be zero terminated.
	*/

	size_t nLength = min( (size * nmemb), 4096 );

	VECBUFFER *pVec = (VECBUFFER *) userp;
	
	char *begin = (char *) ptr;
	char *end = begin + nLength;
	pVec->insert(pVec->end(), begin, end);


#ifdef _DEBUG
	FILE *fp = fopen("cURL_write_callback_DEBUG.txt", "wb");
	fwrite(&(*pVec)[0], 1, pVec->size(), fp);
	fclose(fp);
#endif

	return nLength;
}


CAgentServer* g_pAgentServer = NULL;

CAgentServer::CAgentServer(HWND hWnd, HWND hEditBox, HWND hEditBoxInfo, SUBPATH* pPath, EMSERVICE_PROVIDER ServiceProvier, const bool bEmulator)
	: NetServer(hWnd, hEditBox, hEditBoxInfo, pPath, ServiceProvier)
	, m_bEmulator(bEmulator)
	, m_pClientManager (NULL)
	, m_bTriggerPostCleanUp (TRUE)		// next PostClean MUST be done.
	, m_bTriggerAttendanceMaintenance (TRUE)
	, m_bCooledAttendanceTrigger(TRUE)	// TRUE: now cooled, FALSE: need to be cooled more
    , m_uCntFieldResponse(0)
    , m_wAttendanceResetTimeHour(ATTENDANCE_RESET_TIME_H)
    , m_wAttendanceResetTimeMinute(ATTENDANCE_RESET_TIME_M)
    , m_nServerChannelNumber(1)
    , m_CacheServerSlot(net::INVALID_SLOT)
    , m_SessionServerSlot(net::INVALID_SLOT)
	, m_MatchServerSlot(net::INVALID_SLOT)
{
	MessageEnumMixer::GetInstance();

	sc::LogMan::getInstance()->attachEditControl(hEditBox, hEditBoxInfo);
	initPacketFunc();
	m_bHeartBeat = false;

	for (int i=0; i<MAX_CHANNEL_NUMBER; ++i)
		m_ChannelUser[i] = 0;

    m_pConfig = new AgentServerConfigXml;
    m_pGLAgentServer = new GLAgentServer;

    m_CurrentTime = CTime::GetCurrentTime();
    m_KorShutdownCheckTime = CTime::GetCurrentTime();

	/* curl_global_init() is not thread-safe

	CURLcode cc = curl_global_init(CURL_GLOBAL_ALL);

	if (cc != CURLE_OK)
	{
		sc::writeLogError(sc::string::format("curl_global_init() failure.(%1%)", (int)cc));
	}
	*/

	m_pObservePacketMgr = new GLObservePacketMgr;
	m_pAuthManager = new sc::CGlobalAuthClientLogic;
}

CAgentServer::~CAgentServer()
{
	SAFE_DELETE(m_pClientManager);
    SAFE_DELETE(m_pConfig);
    SAFE_DELETE(m_pGLAgentServer);
	SAFE_DELETE(m_pObservePacketMgr);
	SAFE_DELETE(m_pAuthManager);
	/*
	curl_easy_cleanup(m_curl);
	curl_global_cleanup();
	*/
}

int	CAgentServer::StartClientManager()
{	
	// 클라이언트 정보저장 클래스 생성
	SAFE_DELETE(m_pClientManager);
	m_pClientManager = new NetAgentClientMan(m_nMaxClient, m_hIOServer, m_pConfig->GetReferenceCountMax(), m_FieldServerInfo, m_AgentServerInfo);

	if (!m_pClientManager)
	{
		m_bIsRunning = false;
		return NET_ERROR;
	}

	return NET_OK;
}

int	CAgentServer::StartDbManager()
{
	int nRetCode;

	if ( m_bEmulator == true )
		return NET_OK;

	//
	//mjeon.dbman
	//
    SAFE_DELETE(m_pDbManager);
    m_pDbManager = new COdbcManager(this, db::AGENT_SERVER, m_ServiceProvider, m_pConfig->UseCustomDbPool());
	if (!m_pDbManager)
	{
		sc::writeLogError(std::string("DB:Couldn't get an DbManager Instance."));
		return NET_ERROR;
	}

	// 말레샤 ip차단 %%%
/*
#if defined ( MYE_PARAM ) || defined ( MY_PARAM )
	if( !m_CPLManager.LoadCPLFile() )
	{
		CConsoleMessage::GetInstance()->WriteDatabase( _T("IP BLOCK MANAGER LOAD ERROR") );
		return NET_ERROR;
	}
#endif
*/
	// User DB
	nRetCode = m_pDbManager->OdbcOpenUserDB(
        m_pConfig->GetUserOdbcName(),
        m_pConfig->GetUserOdbcUser(),
        m_pConfig->GetUserOdbcPass(),
        m_pConfig->GetUserOdbcDatabase(),
        m_pConfig->UseCustomDbPool());
	if (nRetCode == sc::db::DB_ERROR)
	{
		sc::writeLogError( "OdbcOpenUserDB" );

		m_bIsRunning = false;
		return NET_ERROR;
	}
	else
	{
		sc::writeLogInfo(std::string("ODBC:User DB Open OK"));
	}
	
	// Game DB
	nRetCode = m_pDbManager->OdbcOpenGameDB(
        m_pConfig->GetGameOdbcName(),
        m_pConfig->GetGameOdbcUser(),
        m_pConfig->GetGameOdbcPass(),
        m_pConfig->GetGameOdbcDatabase(),
        m_pConfig->UseCustomDbPool());
	if (nRetCode == sc::db::DB_ERROR)
	{
        sc::writeLogError( "OdbcOpenGameDB" );

		m_bIsRunning = false;
		return NET_ERROR;
	}
	else
	{	
		sc::writeLogInfo(std::string("ODBC:Game DB Open OK"));
	}	

	// Log DB
	nRetCode = m_pDbManager->OdbcOpenLogDB(
        m_pConfig->GetLogOdbcName(),
        m_pConfig->GetLogOdbcUser(),
        m_pConfig->GetLogOdbcPass(),
        m_pConfig->GetLogOdbcDatabase(),
        m_pConfig->UseCustomDbPool());
	if (nRetCode == sc::db::DB_ERROR)
	{
        sc::writeLogError( "OdbcOpenLogDB" );

		m_bIsRunning = false;
		return NET_ERROR;
	}
	else
	{	
		sc::writeLogInfo(std::string("ODBC:Log DB Open OK"));
	}

	// Shop DB
	nRetCode = m_pDbManager->OdbcOpenShopDB(
        m_pConfig->GetShopOdbcName(),
        m_pConfig->GetShopOdbcUser(),
        m_pConfig->GetShopOdbcPass(),
        m_pConfig->GetShopOdbcDatabase(),
        m_pConfig->UseCustomDbPool());
	if (nRetCode == sc::db::DB_ERROR)
	{
        sc::writeLogError( "OdbcOpenShopDB" );

		m_bIsRunning = false;
		return NET_ERROR;
	}
	else
	{
		sc::writeLogInfo(std::string("ODBC:Shop DB Open OK"));
	}

	// Terra User Database (필리핀/베트남)	
	if (m_ServiceProvider == SP_PHILIPPINES)
	{
		nRetCode = m_pDbManager->OdbcOpenTerraDB(
            m_pConfig->GetTerraOdbcName(),
            m_pConfig->GetTerraOdbcUser(),
            m_pConfig->GetTerraOdbcPass(),
            m_pConfig->GetTerraOdbcDatabase(),
            m_pConfig->UseCustomDbPool());

		if (nRetCode == sc::db::DB_ERROR)
		{
            sc::writeLogError( "OdbcOpenTerraDB" );

			m_bIsRunning = false;
			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("ODBC:Terra DB Open OK"));
		}		
	}

	if (m_ServiceProvider == SP_MALAYSIA)
	{
		nRetCode = m_pDbManager->OdbcOpenTerraDB(
            m_pConfig->GetTerraOdbcName(),
            m_pConfig->GetTerraOdbcUser(),
            m_pConfig->GetTerraOdbcPass(),
            m_pConfig->GetTerraOdbcDatabase(),
            m_pConfig->UseCustomDbPool());

		if (nRetCode == sc::db::DB_ERROR)
		{
            sc::writeLogError( "OdbcOpenTerraDB" );

			m_bIsRunning = false;
			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("ODBC:Terra DB Open OK"));
		}

		nRetCode = m_pDbManager->OdbcOpenMyDB(
            m_pConfig->GetMyOdbcName(),
            m_pConfig->GetMyOdbcUser(),
            m_pConfig->GetMyOdbcPass(),
            m_pConfig->GetMyOdbcDatabase(),
            m_pConfig->UseCustomDbPool());

		if (nRetCode == sc::db::DB_ERROR)
		{
            sc::writeLogError( "OdbcOpenMyDB" );

			m_bIsRunning = false;
			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("ODBC:My DB Open OK"));
		}
	}

	// GSP User Database
	if (m_ServiceProvider == SP_GLOBAL)
	{
		nRetCode = m_pDbManager->OdbcOpenGspDB(
            m_pConfig->GetGspOdbcName(),
            m_pConfig->GetGspOdbcUser(),
            m_pConfig->GetGspOdbcPass(),
            m_pConfig->GetGspOdbcDatabase(),
            m_pConfig->UseCustomDbPool());

		if (nRetCode == sc::db::DB_ERROR)
		{
            sc::writeLogError( "OdbcOpenGspDB" );

			m_bIsRunning = false;
			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("ODBC:Gsp DB Open OK"));
		}
	}

	// Thailand User Database (Thailand)
	if (m_ServiceProvider == SP_THAILAND)
	{
		nRetCode = m_pDbManager->OdbcOpenThaiDB(
            m_pConfig->GetTerraOdbcName(),
            m_pConfig->GetTerraOdbcUser(),
            m_pConfig->GetTerraOdbcPass(),
            m_pConfig->GetTerraOdbcDatabase(),
            m_pConfig->UseCustomDbPool());

		if (nRetCode == sc::db::DB_ERROR)
		{
            sc::writeLogError( "OdbcOpenThaiDB" );

			m_bIsRunning = false;
			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("ODBC:Terra DB Open OK"));
		}
	}

	// Korea Web User DataBase ( Korea )	
	if (m_ServiceProvider == SP_KOREA)
	{
		nRetCode = m_pDbManager->OdbcOpenKorDB(
            m_pConfig->GetKorOdbcName(),
            m_pConfig->GetKorOdbcUser(),
            m_pConfig->GetKorOdbcPass(),
            m_pConfig->GetKorOdbcDatabase(),
            m_pConfig->UseCustomDbPool());

		if (nRetCode == sc::db::DB_ERROR)
		{
            sc::writeLogError( "OdbcOpenKorDB" );

			m_bIsRunning = false;
			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("ODBC:Kor DB Open OK"));
		}
	}

	SAFE_DELETE(m_pAdoManager);
    m_pAdoManager = new db::AdoManager(this, db::AGENT_SERVER, m_ServiceProvider);

	m_pAdoManager->SetConnectionString(
        db::USER_DB,
        m_pConfig->UserDbSource(),
        m_pConfig->UserDbDatabaseName(),
        m_pConfig->UserDbUser(),
        m_pConfig->UserDbPassword(),
		m_pConfig->GetAdoMaxPoolSize(),
		m_pConfig->GetDatabaseTcpPacketSize());    

    m_pAdoManager->SetConnectionString(
        db::GAME_DB,
        m_pConfig->GameDbSource(),
        m_pConfig->GameDbDatabaseName(),
        m_pConfig->GameDbUser(),
		m_pConfig->GameDbPassword(),
		m_pConfig->GetAdoMaxPoolSize(),
		m_pConfig->GetDatabaseTcpPacketSize());

	m_pAdoManager->SetConnectionString(
        db::LOG_DB,
        m_pConfig->LogDbSource(),
        m_pConfig->LogDbDatabaseName(),
        m_pConfig->LogDbUser(),
		m_pConfig->LogDbPassword(),
		m_pConfig->GetAdoMaxPoolSize(),
		m_pConfig->GetDatabaseTcpPacketSize());

    m_pAdoManager->SetConnectionString(
        db::SHOP_DB,
        m_pConfig->ShopDbSource(),
        m_pConfig->ShopDbDatabaseName(),
        m_pConfig->ShopDbUser(),
		m_pConfig->ShopDbPassword(),
		m_pConfig->GetAdoMaxPoolSize(),
		m_pConfig->GetDatabaseTcpPacketSize());

	// 전장서버는 알파와 GS에서만 사용
	if( m_ServiceProvider == SP_KOREA_TEST || m_ServiceProvider == SP_GS || m_ServiceProvider == SP_OFFICE_TEST || m_ServiceProvider == SP_KOREA )
	{
		m_pAdoManager->SetConnectionString(
			db::WORLDBATTLE_RELATED_DB,
			m_pConfig->WorldBattleDbSource(),
			m_pConfig->WorldBattleDbDatabaseName(),
			m_pConfig->WorldBattleDbUser(),
			m_pConfig->WorldBattleDbPassword(),
			m_pConfig->GetAdoMaxPoolSize(),
			m_pConfig->GetDatabaseTcpPacketSize());    
	}

	if( true == m_pConfig->GetUseRanMobile() )
	{
		m_pAdoManager->SetConnectionString(
			db::RANMOBILE_DB,
			m_pConfig->RanMobileDbSource(),
			m_pConfig->RanMobileDbDatabaseName(),
			m_pConfig->RanMobileDbUser(),
			m_pConfig->RanMobileDbPassword(),
			m_pConfig->GetAdoMaxPoolSize(),
			m_pConfig->GetDatabaseTcpPacketSize());    
	}

	return NET_OK;
}

int CAgentServer::Start()
{
    SAFE_DELETE(m_pGLAgentServer);
    m_pGLAgentServer = new GLAgentServer(m_bEmulator);

	int nRetCode = 0;
	HRESULT hResult = S_OK;	

	m_bIsRunning = true;
    m_vecCloseList.clear();

	// ./cfg/server.cfg 파일을 읽어 들인다.
	// 서버 초기화를 진행한다.
	sc::writeLogInfo(std::string("initialize Config."));
    const std::string XmlFileName("AgentServer.xml");    
	if ( m_bEmulator == true )
	{
		::StringCchCopy(m_pConfig->m_sFIELDServer[0][0].szServerName, SERVER_NAME_LENGTH, "Agent");
		::StringCchCopy(m_pConfig->m_sFIELDServer[0][0].szPublicIp, MAX_IP_LENGTH, "127.0.0.1");
		m_pConfig->m_sFIELDServer[0][0].ulPublicIp = ::inet_addr("127.0.0.1");
		::StringCchCopy(m_pConfig->m_sFIELDServer[0][0].szPrivateIP, MAX_IP_LENGTH, "127.0.0.1");
		m_pConfig->m_sFIELDServer[0][0].ulPrivateIP = ::inet_addr("127.0.0.1");
		m_pConfig->m_sFIELDServer[0][0].nServicePort = 5029;
		m_pConfig->m_sFIELDServer[0][0].NetSlotNum = 0;
		m_pConfig->m_sFIELDServer[0][0].bPk = false;
		m_pConfig->m_sFIELDServer[0][0].bLoad = true;
		m_pConfig->m_bAdultCheck = true;
		m_pConfig->m_bUseEventThread = true;
		m_pConfig->m_nFPS = 25;
		m_pConfig->m_bTestServer = false;
		m_pConfig->m_bHeartBeat = false;
		m_pConfig->m_nServerMaxClient = 1000;
		m_pConfig->m_nMaxChannelUser = 1000;
		m_pConfig->m_nPortService = 5502;
		m_pConfig->m_nPortControl = 6001;
		m_pConfig->m_ServerType = net::EMSERVERTYPE(4);
		m_pConfig->m_nServerGroup = 0;
		m_pConfig->m_nServerNumber = 0;	
		m_pConfig->m_ServiceProvider = EMSERVICE_PROVIDER(0);
		m_pConfig->m_nMaxThread = 2;
		m_pConfig->m_LanguageSet = language::LANGFLAG(3);;
		m_pConfig->m_PatchVersion = 0;
		m_pConfig->m_LauncherVersion = 0;
		m_pConfig->m_ServerIp = "127.0.0.1";
		m_pConfig->m_UseCustomOdbcPool = false;
		m_pConfig->m_nReferenceCountMax = 1000;
		m_pConfig->m_nMaxFieldNumber = 1;
		m_pConfig->m_bGameGuardAuth = false;
		m_pConfig->m_nServerChannel = 0;
		m_pConfig->m_nServerChannelNumber = 1;
		m_pConfig->m_bKorShutdown = false;
		m_pConfig->m_bAutoClubJoin = false;
	}
    else if (!m_pConfig->Load(XmlFileName) )
    {
        sc::writeLogError(
            sc::string::format(
                "%1% file load error", XmlFileName));
        m_bIsRunning = false;
        return NET_ERROR;
    }

    m_nServerChannelNumber = m_pConfig->GetServerChannelNumber();    // Agent 가 가진 서버 채널 갯수    
    if (StartCfg(m_pConfig) == NET_ERROR)
        return NET_ERROR;

	//// APEX 버젼 변경 ( 내부서버에서 테스트하기 위해서 추가 )
	//if (m_ServiceProvider == SP_OFFICE_TEST)
	//{
	//	m_ApexClose.clear();

	//	g_pAgentServer = this;
	//	int nRet = APEX_SERVER::StartApexProxy();
	//}
	////////////////////////////////////////////////

	// 각 국가의 로그인에 관련된 컨트롤들을 초기화 한다.
	switch (m_ServiceProvider)
	{
	case SP_KOREA :
		// 다음게임 인증키 초기화
		//m_DaumAuth.Init("Ran72pCsA1yXge3psD63Uu0o3GqpAzyBjm7pcyeODYKefE37fNadf2ng34ejiPja");
		break;
	case SP_JAPAN : 
		// Excite Japan, 암호키 초기화
//		m_MinMd5.ExciteSetSecretKey("XQ1ftLGWZ");
		if ( !(CWhiteRock::GetInstance()->SetWhiteRockXML()) )
		{
			sc::writeLogInfo(std::string("INFO:WhiteRock XML File Load Failed"));

			return NET_ERROR;
		}
		
		// CFG 파일에 설정해둔 WhiteRock Server이름과 Port 세팅

		CWhiteRock::GetInstance()->SetWhiteRockInfo( m_pConfig->GetWhiteRockName(), m_pConfig->GetWhiteRockPort() );

		sc::writeLogInfo(sc::string::format("WhiteRock Address : %s, Port : %d",
			CWhiteRock::GetInstance()->m_sWhiteRockName.GetString(), CWhiteRock::GetInstance()->m_nWhiteRockPort ));

		break;
	case SP_PHILIPPINES :
	case SP_VIETNAM:
		// EGameEncrypt.dll 초기화
		CoInitialize ( NULL );

#if defined ( PH_PARAM ) || defined ( VN_PARAM ) 		
		hResult = m_pIEGameEncrypto.CreateInstance(__uuidof(EGameEncrypt));
#endif

		if (FAILED(hResult))
		{
			sc::writeLogError(std::string("m_pIEGameEncrypto.CreateInstance FAILED"));

			return NET_ERROR;
		}
		else
		{
			sc::writeLogInfo(std::string("m_pIEGameEncrypto.CreateInstance SUCCESS"));
		}
		break;

	case SP_MALAYSIA:
	case SP_MALAYSIA_EN:
		// Malaysia (TERRA) / EGameEncrypt.dll 초기화
		CoInitialize ( NULL );
#if defined ( MY_PARAM) || defined ( MYE_PARAM ) 
		hResult = m_pMyRossoEncrypto.CreateInstance(__uuidof(MyRossoEncrypt));
#endif 

		if (FAILED(hResult))
		{
			sc::writeLogError(std::string("m_pMyRossoEncrypto.CreateInstance FAILED"));

			return NET_ERROR;
		} // if (FAILED(hResult))
		else
		{
			sc::writeLogInfo(std::string("m_pMyRossoEncrypto CreateInstance SUCCESS"));
		}
		break;

	case SP_CHINA:
	case SP_TAIWAN:
	case SP_HONGKONG:
	case SP_THAILAND:
		{
//#if defined( CH_PARAM ) || defined ( HK_PARAM ) || defined ( TW_PARAM ) // || defined(_RELEASED) // Apex 적용
			//if (m_ServiceProvider == SP_TAIWAN || m_ServiceProvider == SP_HONGKONG || m_ServiceProvider == SP_CHINA)
			//{
			m_ApexClose.clear();

			g_pAgentServer = this;
			APEX_SERVER::StartApexProxy();
//#endif
			//}
		}
		break;
	case SP_GS:
		break;
	case SP_INDONESIA:
		break;
	case SP_EU:
		break;
	case SP_US:
		break;
	default :
		break;
	} // switch (m_nServiceProvider)

#ifndef _DEBUG
	
#ifndef _NP30
	// nProtect GameGuard dll 을 초기화 한다.
	DWORD dwGGErrCode = ERROR_SUCCESS;
	dwGGErrCode = InitGameguardAuth("", 1000, FALSE, 0x03);
	if (dwGGErrCode != ERROR_SUCCESS)
	{	
		m_bGameGuardInit = false;
		sc::writeLogError(
            sc::string::format(
                "nProtect GameGuard server DLL load failed. InitGameguardAuth %1%",
                dwGGErrCode));
	}
	else
	{
		m_bGameGuardInit = true;
		sc::writeLogInfo(
            sc::string::format(
                "nProtect GameGuard server DLL load success. InitGameguardAuth %1%",
                dwGGErrCode));

		m_bGameGuardAuth = m_pConfig->GemeGuardAuth();
		if (m_bGameGuardAuth == true)
			sc::writeLogInfo(std::string("nProtect GameGuard Auth ON"));
		else
			sc::writeLogInfo(std::string("nProtect GameGuard Auth OFF"));

        // nProtect 서버 인증을 끈다. 말레이지아 문제.
        m_bGameGuardAuth = false;
	}
#else
	std::string strScriptPath( sc::getAppPath() );
	::SetCurrentDirectory(strScriptPath.c_str());

	UINT32 uReturn = InitCSAuth3("./GameGuard/");
	if (uReturn != ERROR_SUCCESS)
	{	
		m_bGameGuardInit = false;
		sc::writeLogError(
            sc::string::format(
                "nProtect GameGuard server DLL load failed. InitGameguardAuth %1%",
                uReturn));
	}
	else
	{
		m_bGameGuardInit = true;
		sc::writeLogInfo(
            sc::string::format(
                "nProtect GameGuard server DLL load success. InitGameguardAuth %1%",
                uReturn));

		m_bGameGuardAuth = m_pConfig->GemeGuardAuth();
		if (m_bGameGuardAuth == true)
		{
			sc::writeLogDebug(std::string("nProtect GameGuard Auth ON"));
		}
		else
		{
			sc::writeLogDebug(std::string("nProtect GameGuard Auth OFF"));
		}

        // nProtect 서버 인증을 끈다. 말레이지아 문제.
        //m_bGameGuardAuth = false;
	}
#endif

#endif

	// 채널 설정이 올바른지 체크한다.
	if (m_nMaxClient != (m_nMaxChannelUser * m_nServerChannelNumber))
	{
		sc::writeLogError(std::string("MaxChannelUser X ServerChannelNumber = MaxClient"));
		return NET_ERROR;
	}

	// Agent Server Information
	for ( int i=0; i<AGENTSERVER_MAX; ++i )
	{
		A_SERVER_INFO* pAgentInfo = m_pConfig->GetAgentServer( i );
		if ( pAgentInfo )
			m_AgentServerInfo[ i ] = *pAgentInfo;
	}

	if (StartMsgManager(m_nMaxClient) == NET_ERROR)
        return NET_ERROR;

	if (StartIOCP() == NET_ERROR)
        return NET_ERROR;

	if (StartClientManager() == NET_ERROR)
        return NET_ERROR;

	if (StartDbManager() == NET_ERROR)
        return NET_ERROR;

	if (StartWorkThread(m_pConfig) == NET_ERROR)
        return NET_ERROR;

	// Field Server Information
	DWORD dwMaxClient = m_pConfig->GetMaxClient();
	//F_SERVER_INFO* pFieldInfo = m_pConfig->GetFieldServerArray();
    for (int i=0; i<MAX_CHANNEL_NUMBER; ++i)
    {
        for (int j=0; j<FIELDSERVER_MAX; ++j)
        {
            F_SERVER_INFO* pFieldInfo = m_pConfig->GetFieldServer(j, i);
            if (pFieldInfo)
                m_FieldServerInfo[i][j] = *pFieldInfo;
        }
    }

	if( m_pGLAgentServer )
	{
		for ( int i=0; i<AGENTSERVER_MAX; ++i )
		{
			m_pGLAgentServer->CheckAgentServer(
				i, m_AgentServerInfo[ i ] );
		}
	}
	
	sc::CSystemInfo SysInfo;
	DxServerInstance::AgentCreate(
        m_pGLAgentServer,
        SysInfo.GetAppPath(), 
        this,        
        m_pDbManager,	//mjeon.dbman -> StartDbManager() 이후에 m_pDbManager는 유효한 값을 가지게 된다.
		m_pAdoManager,
        m_ServiceProvider,
        m_pPath,
        NULL,
        dwMaxClient,
        &m_FieldServerInfo[0][0],
        (bool) m_bPK,
        m_bPKLess, 
        m_LanguageSet,
        false,
		m_pConfig);

	if (StartUpdateThread() == NET_ERROR)
        return NET_ERROR;
	if (StartListenThread() == NET_ERROR)
        return NET_ERROR;	

	// 태국 대만일경우에만 APEX 스레드 동작
	if(m_ServiceProvider == SP_TAIWAN || m_ServiceProvider == SP_THAILAND)	
	{
		if (StartRegularScheduleThread() == NET_ERROR) // 게임가드 스레드 시작
			return NET_ERROR;
	}
		
	// Connect to Session server
	if ( (SessionConnectSndSvrInfo() == NET_ERROR) && (m_bEmulator == false) )
	{	
		m_bIsRunning = false;
		return NET_ERROR;
	}

#ifdef _USE_AUTH
	//if (m_ServiceProvider == SP_EU)
	//{
		// 세션서버로 해당 서버의 정보를 보낸 다음에 인증처리를 진행해야함.
		if ( SendAuthInfoToSessionServer() == NET_ERROR )
		{
			m_bIsRunning = false;
			return NET_ERROR;	
		}
	//}
#endif

	if (GLUseFeatures::GetInstance().IsUsingMatchSystem())
	{
		std::string strVersion = sc::getFileVersion();
		size_t offset1 = strVersion.find("1.0.");	//alpha
		size_t offset2 = strVersion.find("1.5.");	//next version

		bool bIsAlpha = offset1 != std::string::npos;
		bool bIs1dot5 = offset2 != std::string::npos;

		if (bIsAlpha || bIs1dot5)
		{
			if (MatchConnect() == NET_ERROR)
			{
				sc::writeLogError(std::string("MatchConnect failure."));
				m_bIsRunning = false;
				return NET_ERROR;
			}
		}
	}

	// Connect to all Agent Server
	AgentConnectAll();
	
	// Connect to all field server
	FieldConnectAll();

    // Connect to Cache Server
    if ( (CacheConnect() == NET_ERROR) && (m_bEmulator == false) )
    {	
        m_bIsRunning = false;
        return NET_ERROR;
    }


	//mjeon.연속적인 enum형태로 수정하였으므로 이제는 총 몇개의 메시지가 사용되는지가 의미있다.
	sc::writeLogInfo( sc::string::format("Number of used network messages: %d", NET_MSG_GCTRL_MAX) );	
	//sc::writeLogInfo( sc::string::format("NET_MSG_BASE %d", NET_MSG_BASE) );	

	sc::writeLogInfo(
		sc::string::format(
		"FD_SETSIZE %1%",
		FD_SETSIZE));

	sc::writeLogInfo(std::string(">Server Start OK"));
	sc::writeLogInfo(std::string("======================================================="));

	m_nStatus = S_SERVER_RUNING;

	// 알고 있는 주소에 자신의 주소, Session 서버의 주소를 등록한다.	
    m_pAddressChecker->SetConnectionCountAllow(m_pConfig->GetConnectionPerIpAddress());
    m_pAddressChecker->AddAllowIp(m_pConfig->GetServerIP());
    m_pAddressChecker->AddAllowIp(m_pConfig->GetSessionServerIP());

	// 알고 있는 주소에 필드서버 주소를 등록한다.	
	for (int nCh=0; nCh<MAX_CHANNEL_NUMBER; ++nCh)
	{
		for (int nSvrNum=0; nSvrNum<FIELDSERVER_MAX; ++nSvrNum)
		{			
            m_pAddressChecker->AddAllowIp(m_FieldServerInfo[nCh][nSvrNum].szPublicIp);
            m_pAddressChecker->AddAllowIp(m_FieldServerInfo[nCh][nSvrNum].szPrivateIP);
		}
	}

	// UserDB의 UserInfo Table의 UserLoginState값의 초기화
	InitUserLoginState();

	CheckMaintenanceJobs();

	// http://172.16.2.87/redmine/issues/4577
	if ( m_pConfig->GetAutoClubJoin() )
	{
		m_pGLAgentServer->InsertAutoJoinClub( m_pConfig->GetAutoClubJoinClubDbNum( 0 ) );
		m_pGLAgentServer->InsertAutoJoinClub( m_pConfig->GetAutoClubJoinClubDbNum( 1 ) );
		m_pGLAgentServer->InsertAutoJoinClub( m_pConfig->GetAutoClubJoinClubDbNum( 2 ) );
	}

	return NET_OK;
}

int CAgentServer::Stop()
{
	// Stop All Thread and exit
	DWORD dwExitCode = 0;	

	sc::writeLogInfo(std::string("Please wait until server stop "));

    CloseAllClient();

    if (m_pGLAgentServer)
        m_pGLAgentServer->SaveClubStorage2DbAll();
	
	lockOn();
	m_bIsRunning = false;
	lockOff();

    FieldCloseConnectAll();

	if (m_bUseEventThread)	
	{
		::SetEvent(m_hUpdateQuitEvent);
		::WaitForSingleObject(m_hUpdateThread, INFINITE);
		SAFE_CLOSE_HANDLE(m_hUpdateQuitEvent);
	}
    else
    {
		while (m_hUpdateThread)
			Sleep(0);
	}

	sc::writeLogInfo(std::string("== Update Thread end"));

	DxServerInstance::AgentCleanUp(m_pGLAgentServer);

	StopListenThread();	
	StopWorkThread();
	StopIOCP();

	CacheCloseConnect();
	
	MatchCloseConnect();
	
	SessionCloseConnect();

	// 클라이언트 저장 클래스 종료
	SAFE_DELETE(m_pClientManager);	

	sc::writeLogInfo(std::string("== Client Manager closed"));

	// Message Queue 종료
	SAFE_DELETE(m_pRecvMsgManager);
	
	sc::writeLogInfo(std::string("== Message Manager closed."));

	// DB 메니저 종료
	SAFE_DELETE(m_pDbManager);    

	sc::writeLogInfo(std::string("== Database Manager closed OK"));

#ifndef _DEBUG 
	// nProtect GameGuard 종료
	if (m_bGameGuardInit == true)
	{
#ifndef _NP30
		CleanupGameguardAuth();
#else
		CloseCSAuth3();
		sc::writeLogDebug(std::string("Stop - CloseCSAuth3"));
#endif
	}
#endif

//#if defined( CH_PARAM ) || defined ( HK_PARAM ) || defined ( TW_PARAM ) //|| defined(_RELEASED) // Apex 적용	
	// APEX 버젼 변경
	if (m_ServiceProvider == SP_TAIWAN || m_ServiceProvider == SP_HONGKONG || m_ServiceProvider == SP_CHINA || m_ServiceProvider == SP_THAILAND)
	{
		// Apex 종료
		APEX_SERVER::StopApexProxy();
	}
//#endif

	// Put message to console
	sc::writeLogInfo(std::string("Server Stop OK"));
	sc::writeLogInfo(std::string("======================================================="));
	
	m_nStatus = S_SERVER_STOP;

	return NET_OK;
}

void CAgentServer::ForceStop( DWORD Reason )
{
	sc::writeLogInfo( std::string( "== Please wait until server stop(Force)" ) );
}

/*
int	CAgentServer::Pause()
{
	::closesocket(m_sServer);
	m_sServer = INVALID_SOCKET;
	CloseAllClient();
	m_nStatus = S_SERVER_PAUSE;
	// Put message to console
	sc::writeLogInfo( std::string("======================================================="));
	sc::writeLogInfo( std::string("Server Pause OK"));
	sc::writeLogInfo( std::string("======================================================="));

	return NET_OK;	
}

int CAgentServer::Resume()
{
	if (StartListenThread() == NET_OK)
	{
		m_nStatus = S_SERVER_RUNING;
		// Put message to console
		sc::writeLogInfo( std::string("======================================================="));
		sc::writeLogInfo( std::string("Server Resume OK"));
		sc::writeLogInfo( std::string("======================================================="));

		return NET_OK;
	}
	else
	{
		// Put message to console
		sc::writeLogError( std::string("======================================================="));
		sc::writeLogError( std::string("Server Resume Failed"));
		sc::writeLogError( std::string("======================================================="));

		return NET_ERROR;
	}
}

int	CAgentServer::ReStart()
{
	Stop();
	Start();
	return NET_OK;
}
*/

// close all client connections
void CAgentServer::CloseAllClient()
{
	if (m_pClientManager)
	{
        DWORD MaxClient = static_cast< DWORD >( m_pClientManager->GetMaxClient() );
		for ( DWORD i=1; i<MaxClient; ++i )
		{
			if ( m_pClientManager->IsClientSlot( i ) )
				CloseClient( i, net::LOGOUT_ALL );
		}
	}
}

void CAgentServer::SendMySelf(DWORD dwClient, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc)
{
	NET_MSG_PACK MsgPack;
	if ( MsgPack.SetData(MsgType, PackBuffer, UseCrc) )
		SendMySelf(dwClient, &MsgPack);
}
void CAgentServer::SendMySelf(DWORD dwClient, NET_MSG_GENERIC* pBuffer)
{
	if ( pBuffer )
	{
		PROFILE_MSG_COUNT_SEND(pBuffer);
		InsertMsg(dwClient, pBuffer);
	}
}

int CAgentServer::SendClient(EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, DWORD dwChaNum, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        return SendClient(&MsgPack, dwChaNum);
    else
        return NET_ERROR;
}

//mjeon.AF
int CAgentServer::SendClient(NET_MSG_GENERIC* pBuffer, DWORD dwChaNum)
{
	DWORD dwClient = m_pClientManager->GetClientID(dwChaNum);
	if (dwClient != CLIENTID_NULL)
	    return SendClient(dwClient, pBuffer);
    else
        return NET_ERROR;
}

int CAgentServer::SendClient(DWORD dwClient, NET_MSG_GENERIC* pBuffer)
{
	if (dwClient >= (DWORD) m_nMaxClient || !pBuffer)
	{		
		return NET_ERROR;	
	}
	else
	{
		if (m_pClientManager->IsOnline(dwClient))
		{				
			PROFILE_MSG_COUNT_SEND( pBuffer );
			return m_pClientManager->SendClient(dwClient, pBuffer);
		}
		else
		{			
			return NET_ERROR;
		}
	}
}

int CAgentServer::SendClient(DWORD dwClient, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        return SendClient(dwClient, &MsgPack);
    else
        return NET_ERROR;
}

int CAgentServer::SendField(DWORD dwChannel, DWORD dwFieldSvrNum, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        return SendField(dwChannel, dwFieldSvrNum, &MsgPack);
    else
        return NET_ERROR;
}

int CAgentServer::SendField(DWORD dwChannel, DWORD dwFieldSvrNum, NET_MSG_GENERIC* pBuffer)
{
	GASSERT(pBuffer != NULL);

	GASSERT((dwChannel >= 0 && dwChannel < MAX_CHANNEL_NUMBER) && (dwFieldSvrNum >= 0 && dwFieldSvrNum < FIELDSERVER_MAX));
	if (!(dwChannel >= 0 && dwChannel < MAX_CHANNEL_NUMBER) && (dwFieldSvrNum >= 0 && dwFieldSvrNum < FIELDSERVER_MAX))
	{
		sc::writeLogError(
            sc::string::format(
                "CAgentServer::SendField failure. Channel %1% FieldSvrNum %2% Type %3%",
                dwChannel,
                dwFieldSvrNum,
                pBuffer->Type()));
		return NET_ERROR;
	}
    else
    {	
	    return m_pClientManager->SendField(dwChannel, dwFieldSvrNum, pBuffer);
    }
}

int CAgentServer::SendField(DWORD dwClient, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        return SendField(dwClient, &MsgPack);
    else
        return NET_ERROR;
}

int CAgentServer::SendField(DWORD dwClient, NET_MSG_GENERIC* nmg)
{
	GASSERT((int)dwClient < m_nMaxClient);
	GASSERT(nmg != NULL);

	if (dwClient >= (DWORD) m_nMaxClient || !nmg)
	{
		return NET_ERROR;
	}
	else
	{
		if (m_pClientManager->IsOnline(dwClient))
		{
			return m_pClientManager->SendField(dwClient, nmg);
		}
		else
		{	
			return NET_ERROR;
		}
	}
}

int CAgentServer::SendFieldSvr(int nSvrNum, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, int nServerChannel, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        return SendFieldSvr(nSvrNum, &MsgPack, nServerChannel);
    else
        return NET_ERROR;
}

int CAgentServer::SendFieldSvr(int nSvrNum, NET_MSG_GENERIC* pBuffer, int nServerChannel)
{ 
    return BroadcastToField(nSvrNum, pBuffer, nServerChannel);
}

void CAgentServer::SendAllField(EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, int nServerChannel, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        SendAllField(&MsgPack, nServerChannel);
}

void CAgentServer::SendAllField(NET_MSG_GENERIC* pBuffer, int nServerChannel)
{ 
    BroadcastToField(pBuffer, nServerChannel);
}

int CAgentServer::SendAgent(EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc)
{
	NET_MSG_PACK MsgPack;
	if ( MsgPack.SetData( MsgType, PackBuffer, UseCrc ) )
		return SendAgent( &MsgPack );

    return NET_ERROR;
}

int CAgentServer::SendAgent(NET_MSG_GENERIC* pBuffer)
{
	int nSendCount = 0;

	if ( NULL == pBuffer )
		return NET_ERROR;

	for ( int i=0; i<AGENTSERVER_MAX; ++i )
	{
		// 전장서버에는 보내지 않는다;
		if ( i == m_pConfig->GetWorldBattleServerNumberInfo() )
			continue;

		DWORD dwSlotNum = m_AgentServerInfo[ i ].NetSlotNum;
		if ( dwSlotNum != net::INVALID_SLOT &&
			m_pClientManager->IsOnline( dwSlotNum ) )
		{
			/*sc::writeLogInfo( sc::string::format(
				"Send Message -> Agent IP : %1%, MSG Number : %2%",
				m_pClientManager->GetClientIP( dwSlotNum ),
				pBuffer->Type() ) );*/

			if ( NET_ERROR == SendClient( dwSlotNum, pBuffer ) )
			{
				sc::writeLogError( sc::string::format(
					"SendAgent Slot %1% Type %2%",
					dwSlotNum,
					pBuffer->Type()));
			}
			else
				++nSendCount;
		}
	}	
	
	return nSendCount;
}

//! 특정 Agent;
int CAgentServer::SendAgent ( DWORD dwClient, NET_MSG_GENERIC* pBuffer )
{
	int nSendCount = 0;

	if ( NULL == pBuffer )
		return NET_ERROR;

	if ( dwClient != net::INVALID_SLOT &&
		m_pClientManager->IsOnline( dwClient ) )
	{
		if ( NET_ERROR == SendClient( dwClient, pBuffer ) )
		{
			sc::writeLogError( sc::string::format(
				"SendAgent Slot %1% Type %2%",
				dwClient,
				pBuffer->Type() ) );
		}
	}	

	return NET_OK;
}

int CAgentServer::SendAgent ( DWORD dwClient, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc )
{
	NET_MSG_PACK MsgPack;
	if ( MsgPack.SetData( MsgType, PackBuffer, UseCrc ) )
		return SendAgent( dwClient, &MsgPack );

	return NET_ERROR;
}

int CAgentServer::SendAgent_ServerGroup ( DWORD dwServerGroup, NET_MSG_GENERIC* pBuffer )
{
	int nSendCount = 0;

	if ( NULL == pBuffer )
		return NET_ERROR;

	if ( dwServerGroup < 0 || AGENTSERVER_MAX <= dwServerGroup )
		return NET_ERROR;

	DWORD dwSlotNum = m_AgentServerInfo[ dwServerGroup ].NetSlotNum;

	if ( dwSlotNum != net::INVALID_SLOT &&
		m_pClientManager->IsOnline( dwSlotNum ) )
	{
		if ( NET_ERROR != SendClient( dwSlotNum, pBuffer ) )
		{
			return NET_OK;
		}

		sc::writeLogError( sc::string::format( "SendAgent Slot %1% Type %2%", dwSlotNum, pBuffer->Type() ) );
	}	

	return NET_ERROR;
}

int CAgentServer::SendAgent_ServerGroup ( DWORD dwServerGroup, EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc )
{
	NET_MSG_PACK MsgPack;
	if ( MsgPack.SetData( MsgType, PackBuffer, UseCrc ) )
		return SendAgent_ServerGroup( dwServerGroup, &MsgPack );

	return NET_ERROR;
}

void CAgentServer::SendChannel(EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, int nServerChannel, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        SendChannel(&MsgPack, nServerChannel);
}

void CAgentServer::SendChannel(NET_MSG_GENERIC* pBuffer, int nServerChannel)
{ 
    BroadcastToField(pBuffer, nServerChannel);
}

void CAgentServer::SendAllClient(EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        SendAllClient(&MsgPack);
}

void CAgentServer::SendAllClient(NET_MSG_GENERIC* pBuffer)
{
	sc::writeLogInfo(
        sc::string::format("Message to all player Type %1%", pBuffer->Type()));
    DWORD MaxClient = static_cast<DWORD> (m_nMaxClient);
	for (DWORD i=1; i<MaxClient; ++i)
	{
		// 게임중이면 메시지를 보낸다
		if (m_pClientManager->IsGaming(i))
            SendClient(i, pBuffer);
	}
}

void CAgentServer::SendAllClientChannel(EMNET_MSG MsgType, const msgpack::sbuffer& PackBuffer, int nServerChannel, bool UseCrc)
{
    NET_MSG_PACK MsgPack;
    if (MsgPack.SetData(MsgType, PackBuffer, UseCrc))
        SendAllClientChannel(&MsgPack, nServerChannel);
}

void CAgentServer::SendAllClientChannel(NET_MSG_GENERIC* pBuffer, int nServerChannel)
{
	if (!pBuffer || 
		nServerChannel < 0 || 
		nServerChannel >= m_nServerChannelNumber)
	{
		sc::writeLogError(sc::string::format("SendAllClientChannel CH %1%", nServerChannel));
		return;
	}

	sc::writeLogInfo(sc::string::format("Message to CH %1% players", nServerChannel));

    DWORD MaxClient = static_cast<DWORD> (m_nMaxClient);
	for (DWORD dwStartClient=1; dwStartClient<MaxClient; ++dwStartClient)
	{
		// 게임중이고 채널이 일치한다면 메시지를 보낸다.
		if (m_pClientManager->IsGaming(dwStartClient) && m_pClientManager->GetChannel(dwStartClient) == nServerChannel)
			SendClient(dwStartClient, pBuffer);
	}
}

void CAgentServer::CloseClientCache( DWORD dwClient, net::CLOSE_TYPE emCloseType )
{
	// GQCS 에서 소켓이 닫히더라도 남아 있는 io 개수 만큼 CloseClient 는 호출된다.
	// io 카운트가 0이 될 때 재사용이 가능한 상태이다.
	// io 카운트가 0이 될 때까지 CloseClient에서 CloseClientCache 가 계속 호출 될 것이다.
	int nResult = m_pClientManager->CloseClient( dwClient );
    
	// 연결이 종료 되었을 때만 재연결 시도를 하도록 해야 한다.
	// Cache 서버 통신 종료
	if ( m_bIsRunning && INVALID_SOCKET == m_pClientManager->GetSocket( m_CacheServerSlot ) )
    {
        // 재연결 시도
        sc::writeLogInfo(
            sc::string::format(
            "CacheServer Connection is closed. Trying to reconnect. Current Cache slot is %1%, Close Type %2%", m_CacheServerSlot, emCloseType ) );                
        m_CacheServerSlot = net::INVALID_SLOT;
        CacheConnect();
    }
}

void CAgentServer::CloseClientSession( DWORD dwClient, net::CLOSE_TYPE emCloseType ) 
{
	int nResult = m_pClientManager->CloseClient( dwClient );
    
	// Session 서버와의 통신이 종료되었다.
    if ( m_bIsRunning && INVALID_SOCKET == m_pClientManager->GetSocket( m_SessionServerSlot ) )
    {                
        // Session 서버와 다시 연결을 시도한다.
		sc::writeLogError(
			sc::string::format(
			"SessionServer Connection is closed. Trying to reconnect. Current Session slot is %1%, Close Type %2%", m_SessionServerSlot, emCloseType ) );    
        m_SessionServerSlot = net::INVALID_SLOT;
        SessionReconnect();
    }
}

void CAgentServer::CloseClientAgentField( DWORD dwClient, net::CLOSE_TYPE emCloseType )
{
    // 필드서버와 연결 종료됨
	// 어느 필드가 종료되었는지 표시를
	if ( INVALID_SOCKET != m_pClientManager->GetSocket( dwClient ) )
	{
		DWORD dwChannel = net::INVALID_SLOT;
		DWORD dwFieldNum = net::INVALID_SLOT;
		FindFieldSlot( dwClient, dwChannel, dwFieldNum );
		if ( net::INVALID_SLOT != dwChannel && net::INVALID_SLOT != dwFieldNum )
		{
			m_pGLAgentServer->SetFieldOnline( dwChannel, dwFieldNum, false );
			sc::writeLogError(
				sc::string::format(
				"FieldServer CH: %1%, NUM: %2% Connection is closed. Trying to reconnect. Close Type %3%", dwChannel, dwFieldNum, emCloseType ) );			
		}
		else
		{
			sc::writeLogError( sc::string::format( "FieldServer Connection is closed. Trying to reconnect. Close Type %1%", emCloseType ) );
		}
		m_pGLAgentServer->SENDTOMYSELF(&GLMSG::SNET_SERVER_CLOSE_FIELD(dwChannel, dwFieldNum));
	}
    
	int nResult = m_pClientManager->CloseClient( dwClient );

    // 서버가 운영중이라면 필드서버와 다시 연결시도를 한다.
    if ( m_bIsRunning )
        FieldConnectAll();
}

void CAgentServer::CloseClientClient( DWORD dwClient, net::CLOSE_TYPE emCloseType ) 
{
	if ( INVALID_SOCKET == m_pClientManager->GetSocket( dwClient ) )
	{
		// 아래에 m_pClientManager->CloseClient 가 호출 되었다면 여기로 들어온다.
		// 클라이언트 io 정리를 위해서 CloseClient 호출 한다
		m_pClientManager->CloseClient( dwClient );
		return;
	}

    int		nGameTime= 0;
    int		ChaDbNum  = 0;
    DWORD	dwGaeaID = 0;
    int		nTotalGameTime = 0;
    int		nOfflineTime = 0;
    int		temp = 0;
	
	if (!m_pClientManager->IsClientSlot(dwClient))
	{
		GASSERT(m_pClientManager->IsClientSlot(dwClient)&&"CloseClientClient is closing AgentFieldSlot!!");
		
		return;
	}

    // client 연결 종료
    int UserDbNum = m_pClientManager->UserDbNum(dwClient);
    const char* szUserID = m_pClientManager->GetUserID(dwClient);
	const char* szUserIP = m_pClientManager->GetClientIP(dwClient).c_str();
    int nChannel = m_pClientManager->GetChannel(dwClient);
	dwGaeaID  = m_pClientManager->GetGaeaID(dwClient);
	ChaDbNum   = m_pClientManager->ChaDbNum(dwClient);		

    // 채널 접속자수를 감소시킨다.
    DecreaseChannelUser(nChannel);

    // 등록된 IP 리스트를 제거한다.    
	std::string IpAddress = m_pClientManager->GetClientIP(dwClient);
    m_pAddressChecker->DelConnection(IpAddress);

    if ( UserDbNum > 0 )
    {
        //#if defined(_RELEASED) || defined(TW_PARAM) || defined(TH_PARAM) || defined(HK_PARAM) // ***Tracing Log print
        //if (m_ServiceProvider == SP_OFFICE_TEST || m_ServiceProvider == SP_TAIWAN || m_ServiceProvider == SP_HONGKONG || m_ServiceProvider == SP_THAILAND)
        //{
            int nTracing = CheckTracingUser( m_pClientManager->GetUserID(dwClient), m_pClientManager->UserDbNum(dwClient) );
            if( nTracing != -1 )
            {
                CDebugSet::ToTracingFile(
                    m_vecTracingData[nTracing].m_szAccount,
                    "Account: [%s], UserNum: [%u], IP: [%s]", 
                    m_pClientManager->GetUserID(dwClient),
                    m_pClientManager->UserDbNum(dwClient), 
                    m_pClientManager->GetClientIP(dwClient).c_str());

                CDebugSet::ToTracingFile(
                    m_vecTracingData[nTracing].m_szAccount,
                    "############ Tracing User Logoff ############");

                GLMSG::NET_UPDATE_TRACINGCHAR msg;
                m_vecTracingData[nTracing].dwOnLine = FALSE;
                msg.tracingData = m_vecTracingData[nTracing];
                msg.updateNum   = nTracing;
                SendSession( &msg );
            }
            //#endif
        //}
        nGameTime = m_pClientManager->GetLoginTime(dwClient);


        //#if defined(VN_PARAM) //vietnamtest%%%
        if (m_ServiceProvider == SP_VIETNAM)
        {
            // 마지막 로그오프 시간
            CTime lastOffTime = m_pClientManager->GetLastLoginDate(dwClient);
            CTime crtTime	  = CTime::GetCurrentTime();
            // 년도, 달, 날짜가 다르면 초기화 한다.
            if( lastOffTime.GetYear() != crtTime.GetYear() ||
                lastOffTime.GetMonth() != crtTime.GetMonth() ||
                lastOffTime.GetDay() != crtTime.GetDay()  )
            {
                m_pClientManager->SetVTGameTime(dwClient, 0 );
                temp = 0;
            }
			else
			{
                temp  = m_pClientManager->GetVTGameTime(dwClient);
            }

            CTime tLogin = m_pClientManager->GetLoginTimeEx( dwClient );

            if( tLogin.GetYear() != crtTime.GetYear() ||
                tLogin.GetMonth() != crtTime.GetMonth() ||
                tLogin.GetDay() != crtTime.GetDay()  )

            {
                // 자정에서 지난 시간 ( 분 ) 
                int nMidTime =  ( crtTime.GetHour() * 60 ) + crtTime.GetMinute();

                // nGameTime이 nMidTime보다 크다면 제대로 초기화가 이루어지지 않았다는 얘기이다.
                if ( nGameTime > nMidTime )
                {	
                    nGameTime = nMidTime;				
                }	

            }
            //#else
        }
        else
        {
            temp  = m_pClientManager->GetChinaGameTime(dwClient);
        }
        //#endif 				
        nTotalGameTime = nGameTime+temp;
        nOfflineTime = m_pClientManager->GetChinaOfflineTime(dwClient);

        ///////////////////////////////////////////////////////////////////////////
        // 사용자를 로그아웃 시킨다.
        // 인증을 통과한 상태에서 Close 했을때만 logout 시킨다.
        if (m_pClientManager->IsAccountPass(dwClient) == true)
        {
            m_pDbManager->AddLogJob(
                db::DbActionPtr(
                new CUserLogout(
                szUserID, 
                UserDbNum, 
                nGameTime, 
                ChaDbNum,
                m_nServerGroup,
                m_nServerNum,
                nTotalGameTime,
                nOfflineTime, 
				szUserIP)));

            // APEX 버젼 변경
            if (m_ServiceProvider == SP_TAIWAN || m_ServiceProvider == SP_HONGKONG || m_ServiceProvider == SP_CHINA || m_ServiceProvider == SP_THAILAND)					
            {
                APEX_SERVER::NoticeApexProxy_UserLogout( dwClient, szUserID );
            }
            //#endif

            //#if defined ( JP_PARAM ) //  || defined(_RELEASED) // 일본 WhiteRock과의 통신
            if (m_ServiceProvider == SP_JAPAN)
            {					
                m_pDbManager->AddWebJob(  db::DbActionPtr( new CAgentJapnaWRLogout( m_pClientManager->JPGetUUID(dwClient))));
            }
            //#endif
        }

        ///////////////////////////////////////////////////////////////////////////
        // 캐릭터를 정리한다				
        if (dwGaeaID != GAEAID_NULL)
        {
            // 가이아서버 캐릭터 드랍 예약
            m_pGLAgentServer->ReserveDropOutPC(dwGaeaID); // lock

            // 캐릭터 접속종료 알림
            cp::NET_CHAR_CLOSE_AH MsgAC(UserDbNum, ChaDbNum);
            SendCache(&MsgAC);
        }
        else
        {
            sc::writeLogDebug(sc::string::format("Client Slot %1% Invalid Gaea ID. Close Type %2%", dwClient, emCloseType));
        }

		m_pClientManager->UpdateUserLogoutTime( static_cast< DWORD >( UserDbNum ) );
    }

	{
		// 캐릭터가 Field server 에 혹시 남아 있다면 정리해야 한다.
		// 캐릭터 필드 서버에 잔존 점검 메세지
		GLMSG::SNET_GM_KICK_USER_PROC_FLD MsgAF;
		MsgAF.dwID = ChaDbNum;
		SendChannel(&MsgAF, nChannel);
	}

	
    m_pClientManager->CloseClient( dwClient ); // 클라이언트와 연결종료
	m_pClientManager->ResetFieldInfo(dwClient);	
	
    //m_pClientManager->CloseField (dwClient); // 필드서버와 연결종료			

    SessionSndSvrCurState();

    // 만일 현재 Reached Max Client 상태이면...
    if( m_bReachedMAX )
    {
        sc::writeLogInfo( sc::string::format("NET_MSG_BASE %d", dwClient) );		
        m_bReachedMAX = FALSE;
    }
}

void CAgentServer::CloseClient( DWORD dwClient, net::CLOSE_TYPE emCloseType, int nLastError)
{
	lockOn();

	if ( dwClient >= static_cast< DWORD >( m_nMaxClient ) )
	{
		sc::writeLogError( sc::string::format( "CloseClient wrong dwClient is %d, Close Type %d, Error Type %d", dwClient, emCloseType, nLastError ) );
		lockOff();
		return;
	}

	sc::writeLogInfo( sc::string::format( "Connection close. SlotType %1%, UserNum %2%, CloseType %3%, ErrorType %4%", m_pClientManager->GetSlotType( dwClient ), m_pClientManager->UserDbNum(dwClient), emCloseType, nLastError ) );

	switch ( m_pClientManager->GetSlotType( dwClient ) )
	{
	case net::SLOT_CACHE:
		// Cache server
		CloseClientCache( dwClient, emCloseType );
		break;
	case net::SLOT_SESSION:
		// Session server
		CloseClientSession( dwClient, emCloseType );
		break;
	case net::SLOT_MATCH:
		// Match server
		MatchCloseConnect();
		break;
	case net::SLOT_AGENT_FIELD:
		// Field server
		CloseClientAgentField( dwClient, emCloseType );
		break;
	case net::SLOT_CLIENT:
		// client
		CloseClientClient( dwClient, emCloseType );		
		break;
	case net::SLOT_AGENT_AGENT:
		// Agent Server
		CloseClientAgentAgent( dwClient, emCloseType );
		break;
	default:
		// 알수없는 연결 끊김
		sc::writeLogError( sc::string::format( "Unknown reason connection close. Close Type %1%, Error Type %2%", emCloseType, nLastError ) );
		break;
	}

	lockOff();
}

bool CAgentServer ::IsSlotOnline(DWORD dwClient)
{
	return m_pClientManager->IsOnline(dwClient);
}

int CAgentServer::GetOnlineFieldCount()
{
    int onlineCounter = 0;

    for (size_t i=0; i<MAX_CHANNEL_NUMBER; ++i)
    {
        for (size_t j=0; j<FIELDSERVER_MAX; ++j)
        {
            DWORD SlotNum = m_FieldServerInfo[i][j].NetSlotNum;
            if (SlotNum != net::INVALID_SLOT && m_pClientManager->IsOnline(SlotNum))
            {
                onlineCounter++;
            }
        }
    }

    sc::writeLogDebug( sc::string::format( "GetOnlineFieldCount(), value is %d", onlineCounter ) );

    return onlineCounter;
}

/*
void CAgentServer::CloseField(DWORD dwClient)
{	
	GASSERT(dwClient<(DWORD)m_nMaxClient);
    if ((dwClient >= (DWORD) m_nMaxClient))
	{
		return;
	}
    else
	{
		//m_pClientManager->CloseField(dwClient);
		m_pClientManager->ResetFieldInfo(dwClient);        
	}
}
*/

//mjeon.AF
int CAgentServer::SetUpFieldInfo(DWORD dwClient, int nFieldServer, DWORD dwGaeaID, int nServerChannel)
{
	F_SERVER_INFO* pFieldInfo = GetFieldServerInfo(nFieldServer, nServerChannel);
    if (pFieldInfo)
        return m_pClientManager->SetUpFieldInfo(dwClient, nFieldServer, dwGaeaID, nServerChannel);
    else
	{
		sc::writeLogError(std::string("CAgentServer::SetUpFieldInfo() - Cannot retrieve FieldInfo."));
        return NET_ERROR;
	}
}

//mjeon.AF
int CAgentServer::SetUpInstanceInfo(DWORD dwClient, int nInstanceServer, int nServerChannel)
{
	return m_pClientManager->SetUpInstanceInfo(dwClient, nInstanceServer, nServerChannel);
}
/*
int CAgentServer::ConnectFieldSvr(DWORD dwClient, int nFieldServer, DWORD dwGaeaID, int nServerChannel)
{
    F_SERVER_INFO* pFieldInfo = GetFieldServerInfo(nFieldServer, nServerChannel);
    if (pFieldInfo)
        return m_pClientManager->ConnectFieldServer(dwClient, nFieldServer, pFieldInfo, dwGaeaID, nServerChannel);
    else
        return NET_ERROR;	    
}
*/

/*
BOOL CAgentServer::IsBoundByInstance(DWORD dwClient, DWORD &dwInstanceSvrNum)
{
	return m_pClientManager->IsBoundByInstance(dwClient, OUT dwInstanceSvrNum);
}
*/

int CAgentServer::ExecuteCommand(char* strCmd)
{
    if (!strCmd)
        return NET_ERROR;

    std::string strCommand(strCmd);
    std::vector<std::string> SplitVec;
    boost::split(SplitVec, strCommand, boost::algorithm::is_any_of(" "));
    if (SplitVec.empty())
        return NET_ERROR;

	if (SplitVec[0] == "print_ip")
	{
		// SERVER_UTIL::CAddressChecker::GetInstance()->Save();
	}
	else if (SplitVec[0] == "print_freeslot")
	{
		m_pClientManager->SaveFreeSlot();
	}
	else if (SplitVec[0] == "print_info")
	{
		// SERVER_UTIL::CAddressChecker::GetInstance()->Save();
		m_pClientManager->SaveFreeSlot();
	}		
	// 서버 채팅 메시지, global
	else if (SplitVec[0] == "chat")
	{		
		if (SplitVec.size() > 1)
			MsgSndChatGlobal(SplitVec[1].c_str());

	}	
	else if (SplitVec[0] == "profile_on")
	{
        // PROFILE 사용.
		g_bProfile = TRUE;
		sc::writeLogInfo(std::string("PROFILE ON"));
		return 0;
	}	
	else if (SplitVec[0] == "profile_off")
	{
        // PROFILE 사용안함.
		g_bProfile = FALSE;
		sc::writeLogInfo(std::string("PROFILE OFF"));
	}
    else
    {
    }
	return 0;
}

void CAgentServer::CheckMaintenanceJobs()
{
	//
	// 서버 시간이 변경되지 않은 이상, sp_AttendanceMaintenance는 1일 1회만 수행되는 것을 보장한다.
	//
	Ready4AttendanceMaintenance(TRUE);

    AddGameAdoJob(
        db::DbActionPtr(
			new CAttendanceMaintenance()));
}

void CAgentServer::InitUserLoginState()
{
	int nServerGroup	= 0;

	nServerGroup	= m_pConfig->GetServerGroup();

	m_pDbManager->AllServerUserLogout(m_nServerGroup);
}

int CAgentServer::CheckTracingUser(const char *pUserAccount, const int nUserNum)
{
	for (size_t i = 0; i < m_vecTracingData.size(); ++i)
	{
		const STracingData& data = m_vecTracingData[i];
		if (_strcmpi(data.m_szAccount, pUserAccount) == 0 || data.dwUserNum  == nUserNum)
			return static_cast<int> (i);
	}
	return -1;
}


void CAgentServer::DailyMaintenance()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	PostCleanUp(st);

	AttendanceDailyMaintenance(st);
}


//
//mjeon
// 범용으로 time-event 처리를 하기위한 별도의 class를 하나 만들어서 아래 코드를 대체하도록 하자.
//
BOOL CAgentServer::AttendanceDailyMaintenance(SYSTEMTIME &st, BOOL bManually)
{
	if (!GLUseFeatures::GetInstance().IsUsingAttendance())
	{
		//sc::writeLogError(std::string("AttendanceDailyMaintenance() - Wrong use of Attendance while it is OFF!"));
		return FALSE;
	}

	//
	// 일일정리 시간이 되었거나, GM명령에 의한 호출인가?
	//
	if ( (st.wHour == ATTENDANCE_RESET_TIME_H && st.wMinute == ATTENDANCE_RESET_TIME_M) || bManually )
	{
		if ( m_bTriggerAttendanceMaintenance )	//현재 AttendanceMaintenance를 진행 중이 아닌가?
		{	
			if (m_bCooledAttendanceTrigger)		//이전 AttendanceMaintenance로부터 일정한 cooltime이 지났는가?
			{
				if (!bManually)
					sc::writeLogInfo(std::string("Attendance Daily Maintenance STARTS."));
				else
					sc::writeLogInfo(std::string("Attendance Maintenance MANUALLY STARTS."));

				m_bTriggerAttendanceMaintenance	= FALSE;	//Ready4AttendanceMaintenance(TRUE);
				m_bCooledAttendanceTrigger		= FALSE;
                m_wAttendanceResetTimeHour      = st.wHour;
                m_wAttendanceResetTimeMinute    = st.wMinute;
				
				GLMSG::SNET_ATTENDANCE_DAILYMAINTENANCE_START_AF msg;
				
				SendAllChannel(&msg);	//notify to all fieldServers
			}
			/*
			else
			{
				sc::writeLogError(std::string("Attendance Daily Maintenance COULD NOT STARTED because of COOLTIME."));
			}
			*/
		}
		/*
		else
		{
			sc::writeLogError(std::string("Attendance Daily Maintenance COULD NOT STARTED because previous maintenance has not been finished yet."));
		}
		*/
	}

    WORD coolTimeHour = m_wAttendanceResetTimeHour;
    WORD coolTimeMinute = m_wAttendanceResetTimeMinute + ATTENDANCE_RESET_COOLTIME_M;

    if( coolTimeMinute >= 60 ) // 수동 메인테넌스 진행 등으로, 60분이 되어, 시간 단위가 넘어갈 경우
    {
        coolTimeMinute -= 60;
        coolTimeHour = (coolTimeHour + 1) % 24;
    }

	if ( st.wHour == coolTimeHour && st.wMinute == coolTimeMinute )
	{
        if( !m_bCooledAttendanceTrigger )
        {
            if( !m_bTriggerAttendanceMaintenance ) // 쿨다운이 되었는데도, 현재 AttendanceMaintenance가 끝나지 않았는가?
            {
                // 응답하지 않는 필드서버는 죽었다고 가정, 강제로 Maintenance를 진행한다.

                sc::writeLogError(std::string("Some field server didn't send response. Attendance Daily Maintenance will forcibly start."));

                //
                // start AttendanceDailyMaintenance
                //
                AddGameAdoJob(
                    db::DbActionPtr(
                    new CAttendanceMaintenance()));
                m_uCntFieldResponse = 0;
            }

            //sc::writeLogDebug(std::string("Attendance Daily Maintenance Cooldown reset OK."));
            m_bCooledAttendanceTrigger = TRUE;
        }
	}

	return TRUE;
}


void CAgentServer::PostCleanUp(SYSTEMTIME &st)
{
	if (!GLUseFeatures::GetInstance().IsUsingPost())
	{
		//sc::writeLogError(std::string("PostCleanUp() - Wrong use of postsystem while it is OFF!"));
		return;
	}

	//
	//mjeon.Post
	//
	//Clean up old posts.
	//Post-CleanUp will be triggered everyday at 5:00am.

	//SYSTEMTIME st;
	//GetLocalTime(&st);

	if ( m_bTriggerPostCleanUp && st.wHour == 5 && st.wMinute == 0 )
	{
		m_bTriggerPostCleanUp = FALSE;

		//CPostClean *pDbAction = new  CPostClean();
		AddGameAdoJob(
            db::DbActionPtr(
                new CPostMaintenance));
	}

	//
	// Following flagging can be done by msg from DBAction(CPostClean) after it has done.
	// Both will have same result with this routine.
	//
	if ( m_bTriggerPostCleanUp == FALSE && st.wHour == 5 && st.wMinute > 0 )
	{
		m_bTriggerPostCleanUp = TRUE;	//reset
	}
}

void CAgentServer::RecvMsgProcess()
{
	MSG_LIST* pMsg = NULL;	

	// Flip Messge Queue
	m_pRecvMsgManager->MsgQueueFlip();

 	// Get Messages and Processing message
	pMsg = m_pRecvMsgManager->GetMsg( NULL );
	while (pMsg != NULL)
	{
		// 메시지 처리
		MsgProcess(pMsg);
		pMsg = m_pRecvMsgManager->GetMsg( pMsg ); // 다음메시지 가져오기
	}

}

void CAgentServer::AgentSrvHeartBeatCheck( DWORD dwUdpateTime )
{
	DWORD dwCurrentTime	= timeGetTime();
	
    // User Connection Check
	if ((dwCurrentTime - m_dwHeartBeatTime) > SESSION_CHECK_TIME)
	{
		if (m_bHeartBeat)
            ServerHeartBeatRequest();
		else
            ServerHeartBeatCheck();

		if (m_bGameGuardInit && m_bGameGuardAuth)
            ClientNprotectCheck();
		else
            ClientHeartBeatCheck();

        if (m_pGLAgentServer)
            m_pGLAgentServer->TxtLogToDb();

		m_bHeartBeat = !m_bHeartBeat;
		m_dwHeartBeatTime = dwCurrentTime;

        sc::SeqUniqueGUID::Instance()->reset();
	}
}

void CAgentServer::ClientHeartBeatCheck() 
{
	DWORD MaxClient = static_cast< DWORD >( m_nMaxClient );
    if ( m_bHeartBeat )
    {	
        sc::writeInfo( std::string( "Send Heartbeat Request" ) );
        NET_HEARTBEAT_CLIENT_REQ HeartbeatClientRequest;
        for ( DWORD dwN=1; dwN<MaxClient; ++dwN )
        {
            // 온라인 상태인경우
            if ( INVALID_SOCKET != m_pClientManager->GetSocket( dwN ) && m_pClientManager->IsClientSlot( dwN ) )
            {
                m_pClientManager->ResetHeartBeat( dwN );
                SendClient( dwN, &HeartbeatClientRequest );
            }
        }
    }
    else
    {
        sc::writeInfo( std::string( "Check Heartbeat" ) );
        for ( DWORD dwN=1; dwN<MaxClient; ++dwN )
        {	
            if ( 
				INVALID_SOCKET != m_pClientManager->GetSocket( dwN ) && 
				m_pClientManager->IsClientSlot( dwN ) && 
				!m_pClientManager->CheckHeartBeat( dwN ) )
				CloseClient( dwN, net::KICKOUT_HEARTBEAT );
        }
    }
}

void CAgentServer::ClientNprotectCheck() 
{
    // GameGuard Check
    if ( m_bHeartBeat )
    {
        sc::writeInfo( std::string( "Send GameGuardAuth Request" ) );
        // 게임가드 인증용 메시지
        NET_GAMEGUARD_AUTH nga;
        for ( DWORD dwN=1; dwN<( DWORD ) m_nMaxClient; ++dwN )
        {
			if ( INVALID_SOCKET != m_pClientManager->GetSocket( dwN ) && m_pClientManager->IsClientSlot( dwN ) )
            {
#ifndef _NP30
                m_pClientManager->ResetHeartBeat( dwN );
                MsgSndGameGuard (dwN, nga );
#else
				if( m_pClientManager->nProtectIsFirstCheck(dwN) == false )
				{
					m_pClientManager->ResetHeartBeat( dwN );
					MsgSndGameGuard (dwN, nga );

					//sc::writeLogDebug( sc::string::format( "ClientNprotectCheck :MsgSndGameGuard - clientID %1%, packetSize %2%", dwN, nga.packetSize ) );
				}
#endif
            }
        }
    }
    else
    {
        sc::writeInfo( std::string( "Check GameGuardAuth" ) );

        DWORD dwResult = 0;
		NET_GAMEGUARD_AUTH nga;

        for ( DWORD dwN=1; dwN<( DWORD ) m_nMaxClient; ++dwN )
        {
            if ( INVALID_SOCKET != m_pClientManager->GetSocket( dwN ) && m_pClientManager->IsClientSlot( dwN ) )
            {
#ifndef _NP30
                dwResult = m_pClientManager->nProtectCheckAuthAnswer( dwN );
                if ( dwResult != 0 && !m_pClientManager->CheckHeartBeat( dwN ) )
                {
                    WriteNprotectLog( dwN, dwResult );
					CloseClient( dwN, net::KICKOUT_GAMEGUARD );
                }
#else
				if( m_pClientManager->nProtectIsFirstCheck(dwN) == false )
				{
					m_pClientManager->ResetHeartBeat( dwN );
					MsgSndGameGuard (dwN, nga );

					//sc::writeLogDebug( sc::string::format( "ClientNprotectCheck :MsgSndGameGuard - clientID %1%, packetSize %2%", dwN, nga.packetSize ) );
				}
#endif
            }
        }
    }
}

void CAgentServer::ServerHeartBeatCheck() 
{
    if (!m_bIsRunning)
        return;

    // Session check
    if ( m_SessionServerSlot != net::INVALID_SLOT && 
		INVALID_SOCKET != m_pClientManager->GetSocket( m_SessionServerSlot ) && 
		!m_pClientManager->CheckHeartBeat( m_SessionServerSlot ) )
    {
        sc::writeLogError( std::string( "Session server heartbeat not response!. will try reconnect!" ) );
		CloseClient( m_SessionServerSlot, net::KICKOUT_HEARTBEAT );
    }

    // Cache check
    if ( m_CacheServerSlot != net::INVALID_SLOT && 
		INVALID_SOCKET != m_pClientManager->GetSocket( m_CacheServerSlot ) && 
		!m_pClientManager->CheckHeartBeat( m_CacheServerSlot ) )
    {
        sc::writeLogError( std::string( "Cache server heartbeat not response!. will try reconnect!" ) );
        CloseClient( m_CacheServerSlot, net::KICKOUT_HEARTBEAT );
    }


	// Match check
    if ( m_MatchServerSlot != net::INVALID_SLOT && 
		INVALID_SOCKET != m_pClientManager->GetSocket( m_MatchServerSlot ) && 
		!m_pClientManager->CheckHeartBeat( m_MatchServerSlot ) )
    {
        sc::writeLogError( std::string( "Match server heartbeat not response!. will try reconnect!" ) );
        CloseClient( m_MatchServerSlot, net::KICKOUT_HEARTBEAT );
    }

    // Field Server Connection Check
    for ( int i=0; i<MAX_CHANNEL_NUMBER; ++i )
    {
        for ( int j=0; j<FIELDSERVER_MAX; ++j )
        {
            DWORD SlotNum = m_FieldServerInfo[i][j].NetSlotNum;            
            if ( SlotNum != net::INVALID_SLOT && 
				INVALID_SOCKET != m_pClientManager->GetSocket( SlotNum ) && 
				!m_pClientManager->CheckHeartBeat( SlotNum ) )
                CloseClient( SlotNum, net::KICKOUT_HEARTBEAT );
        }
    }

	// Agent Server Connection Check
	for ( int i=0; i<AGENTSERVER_MAX; ++i )
	{
		DWORD SlotNum = m_AgentServerInfo[ i ].NetSlotNum;            
		if ( SlotNum != net::INVALID_SLOT && 
			INVALID_SOCKET != m_pClientManager->GetSocket( SlotNum ) && 
			!m_pClientManager->CheckHeartBeat( SlotNum ) )
			CloseClient( SlotNum, net::KICKOUT_HEARTBEAT );
	}
}

void CAgentServer::ServerHeartBeatRequest() 
{
    NET_HEARTBEAT_SERVER_REQ HeartbeatServerRequest;

    // Session server connection check
    if (m_SessionServerSlot != net::INVALID_SLOT && m_pClientManager->IsOnline(m_SessionServerSlot))
    {
        m_pClientManager->ResetHeartBeat(m_SessionServerSlot);
        SendSession(&HeartbeatServerRequest);
    }
    else
    {
        if (m_bIsRunning)
        {
            sc::writeLogError(std::string("Reconnecting session Server"));
            SessionReconnect();
            SessionConnectSndSvrInfo();
        }
    }

    // Cache server connection check
    if (m_CacheServerSlot != net::INVALID_SLOT && m_pClientManager->IsOnline(m_CacheServerSlot))
    {
        m_pClientManager->ResetHeartBeat(m_CacheServerSlot);
        SendCache(&HeartbeatServerRequest);
    }
    else
    {
        if (m_bIsRunning)
        {
            sc::writeLogError(std::string("Reconnecting cache server"));
            CacheConnect();
        }
    }

    // Field Server Connection Check
    // 서버의 채널 갯수와 필드서버 갯수를 가져온다.    
    for (size_t i=0; i<MAX_CHANNEL_NUMBER; ++i)
    {
        for (size_t j=0; j<FIELDSERVER_MAX; ++j)
        {
            DWORD SlotNum = m_FieldServerInfo[i][j].NetSlotNum;
            if (SlotNum != net::INVALID_SLOT)
            {
                m_pClientManager->ResetHeartBeat(SlotNum);
                SendClient(SlotNum, &HeartbeatServerRequest);
            }
        }
    }

	// Agent Server Connection Check
	for ( size_t i=0; i<AGENTSERVER_MAX; ++i )
	{
		DWORD dwSlotNum = m_AgentServerInfo[ i ].NetSlotNum;
		if ( dwSlotNum != net::INVALID_SLOT )
		{
			m_pClientManager->ResetHeartBeat( dwSlotNum );
			SendClient( dwSlotNum, &HeartbeatServerRequest );
		}
	}
}

void CAgentServer::UpdateClientState()
{
    // 한국 셧다운제 체크
    if ((m_ServiceProvider == SP_OFFICE_TEST || m_ServiceProvider == SP_KOREA || m_ServiceProvider == SP_KOREA_TEST) &&
        m_pConfig->GetKorShutdown())
    {
        __time64_t ElspTime = m_CurrentTime.GetTime() - m_KorShutdownCheckTime.GetTime();
        if (_abs64(ElspTime) > 60)
        {
            m_KorShutdownCheckTime = m_CurrentTime;

            // 셧다운제 0~6시
            if (m_CurrentTime.GetHour() >= 0 && m_CurrentTime.GetHour() < 6)
            {
                DWORD MaxClient = static_cast<DWORD> (m_nMaxClient);
                for (DWORD i=1; i<MaxClient; ++i)
                {
                    if ( INVALID_SOCKET != m_pClientManager->GetSocket( i ) && m_pClientManager->IsClientSlot( i ) )
                    {
                        // 셧다운제 16세
                        if (m_pClientManager->GetAge(i) < 16)
                        {
                            sc::writeLogInfo(
                                sc::string::format(
                                    "CAgentServer::UpdateClientState Shutdown age %1%",
                                    m_pClientManager->GetAge(i)));
							CloseClient( i, net::KICKOUT_SHUTDONW_RULE );
                        }   
                    }
                }
            }
            else if (m_CurrentTime.GetHour() == 23 && m_CurrentTime.GetMinute() > 49)
            {
                DWORD MaxClient = static_cast<DWORD> (m_nMaxClient);
                for (DWORD i=1; i<MaxClient; ++i)
                {
                    if ( INVALID_SOCKET != m_pClientManager->GetSocket( i ) && m_pClientManager->IsClientSlot( i ) )
                    {
                        // 셧다운제 16세
                        if (m_pClientManager->GetAge(i) < 16)
                        {
                            GLMSG::NET_KOR_SHUTDOWN_WARNING_AC MsgAC;
                            SendClient(i, &MsgAC);
                        }   
                    }
                }                
            }
            else
            {

            }
        }
    }
	
/*
	// 베트남 밤 12시가 지나서 전체 시간을 초기화 함

	if (m_ServiceProvider == SP_VIETNAM)
	{
		CTime crtTime = CTime::GetCurrentTime();
		if( m_CurrentTime.GetDay() != crtTime.GetDay() ||
			m_CurrentTime.GetMonth() != crtTime.GetMonth() ||
			m_CurrentTime.GetYear() != crtTime.GetYear()  )
		{
			DWORD dwGaeaID;
			m_CurrentTime	= CTime::GetCurrentTime();
            DWORD MaxClient = static_cast<DWORD> (m_nMaxClient);
			for (DWORD dwVNCount=1; dwVNCount<MaxClient; ++dwVNCount)
			{
				if (m_pClientManager->IsOnline(dwVNCount) && m_pClientManager->IsClientSlot(dwVNCount))
				{
					dwGaeaID = m_pClientManager->GetGaeaID(dwVNCount);
					GLCharAG* pChar = m_pGLAgentServer->GetChar(dwGaeaID);

					// 로비에 있을경우 pChar가 널이다. 
					m_pClientManager->SetVTGameTime(dwVNCount,0);
					m_pClientManager->SetLoginTime(dwVNCount);

					if (!pChar)
					{
						sc::writeLogError(
                            sc::string::format(
                                "pChar Null. dwGaeaID %1% dwVNCount %2% VTGameTime %3%", 
							    dwGaeaID,
                                dwVNCount,
                                m_pClientManager->GetVTGameTime(dwVNCount)));
					}
                    else
                    {
					    pChar->m_sVietnamSystem.gameTime = 0;
					    pChar->m_sVietnamSystem.loginTime = m_CurrentTime.GetTime();
                    }
				}
			} 

			GLMSG::SNETPC_VIETNAM_ALLINITTIME NetMsg;
			NetMsg.initTime = m_CurrentTime.GetTime();
			//					m_pGLAgentServer->MsgProcess( &NetMsg );
			SendAllChannel ( &NetMsg );
            sc::writeLogInfo( std::string("INFO:-=-=-=-=Init Vietnam Time=-=-=-=-=-") );
		}	
	}
*/
}

void CAgentServer::PrintDebugMsg( DWORD dwUdpateTime )
{
	DWORD dwCurrentTime = timeGetTime();
	///////////////////////////////////////////////////////////////////////
	// 프레임 측정
	if ((dwCurrentTime - m_dwFrameTime) >= dwUdpateTime )
	{	

		// 프로파일링 
		DWORD dwProfileStart = timeGetTime();
		PROFILE_DUMPOUTPUT_FILE();
		DWORD dwProfileEnd = timeGetTime() - dwProfileStart;

		// 처리시간이 500msec 이상이라면 기록한다.
		if (dwProfileEnd > 500)
		{
			sc::writeLogInfo( sc::string::format("INFO:PROFILE_DUMPOUTPUT_FILE %d msec", dwProfileEnd) );
		}

		// DB 연결유지				
		// m_pDbManager->CheckConnection();

		float fFrame = ((float)((m_dwFrameCount * dwUdpateTime) /(float)(timeGetTime() - m_dwFrameTime)));			
		m_dwFrameCount = 0;				

		// 현재 사용자수 디스플레이

		// 프레임 측정 디스플레이		
		sc::writeInfo(
            sc::string::format(
                "Update %.2f fps. User %d/%d",
                (fFrame/(dwUdpateTime/1000)),
                (int) m_pClientManager->GetCurrentClientNumber(),
                (int) m_pClientManager->GetMaxClient()));

		// 패킷 처리량 디스플레이
		sc::writeInfo(
            sc::string::format(
            "In Packet Count %d/sec Traffic %d(bytes)/sec",
            (int) (m_pClientManager->getInPacketCount() / (dwUdpateTime/1000)),
            (int) (m_pClientManager->getInPacketSize() / (dwUdpateTime/1000)) ));

		sc::writeInfo(
            sc::string::format(
                "Out Packet Count %d/sec Traffic %d(bytes)/sec",
				(int) (m_pClientManager->getOutPacketCount() / (dwUdpateTime/1000)),
				(int) (m_pClientManager->getOutPacketSize() / (dwUdpateTime/1000))));
		

		sc::writeInfo(
            sc::string::format(
                "Compress Count %d/sec Traffic %d(bytes)/sec",
                (int) (m_pClientManager->getOutCompressCount() / (dwUdpateTime/1000)),
                (int) (m_pClientManager->getOutCompressSize() / (dwUdpateTime/1000))));

		// Message Queue / IOCP 메모리 사용량 디스플레이		
		sc::writeInfo(
            sc::string::format(
				"Msg Queue Size %1%", // Send %2% Recv %3%",
				m_pRecvMsgManager->GetCount()
                //m_pClientManager->GetUseSendMemoryCount(),
                //m_pClientManager->GetUseRecvMemoryCount()
				));


		// 채널별 사용자수 디스플레이
		sc::writeInfo(
            sc::string::format(
                "CH[%d %d %d %d %d %d %d %d %d %d]",
                m_ChannelUser[0],
                m_ChannelUser[1],
                m_ChannelUser[2],
                m_ChannelUser[3],
                m_ChannelUser[4],
                m_ChannelUser[5],
                m_ChannelUser[6],
                m_ChannelUser[7],
                m_ChannelUser[8],
                m_ChannelUser[9]));

		// 패킷카운터 리셋
		m_pClientManager->resetPacketCount();

		// 임시로 여기에 좀 하자
		m_pClientManager->CleanDummyClientSlot();

		m_dwFrameTime = timeGetTime();
	}
	else
	{
		m_dwFrameCount++;

		DWORD dwProfileDumpStart = timeGetTime();
		g_profile_1.DumpOutputToNon();
		DWORD dwProfileDumpEnd = timeGetTime() - dwProfileDumpStart;
		if (dwProfileDumpEnd > 500)
			sc::writeLogInfo( sc::string::format("INFO:PROFILE::DumpOutputToNon() %d msec", dwProfileDumpEnd) );
	}
}

void CAgentServer::EndUpdateThread()
{
	// Last db action
    if (m_pAdoManager)
        m_pAdoManager->SetAllCharacterOffline(); // 모든 캐릭터를 오프라인으로 만든다.

    if (m_pDbManager)
	    m_pDbManager->AllServerUserLogout(m_nServerGroup);

    if (m_pDbManager)
        m_pDbManager->ExecuteLastJob();    

	if (m_pAdoManager)
		m_pAdoManager->ExecuteLastJob();    

	TRACE("[UpdateProc End]\n");

	// 업데이트 스래드 사라짐.
	CloseHandle(m_hUpdateThread);
	m_hUpdateThread = NULL;
	m_bUpdateEnd	= true;
}

void CAgentServer::ReserveCloseClient( DWORD dwClient )
{
    if ( dwClient < ( UINT ) m_nMaxClient && m_pClientManager->IsClientSlot( dwClient ) )
    {
        if ( !IsReserveCloseClient( dwClient ) )
            m_vecCloseList.push_back( dwClient );
    }
}

void CAgentServer::CloseClientFinal()
{    
    size_t CloseSize = m_vecCloseList.size();
    for ( size_t i=0; i<CloseSize; ++i )
		CloseClient( m_vecCloseList[i], net::KICKOUT_PACKET_COUNT );
    m_vecCloseList.clear();
}

bool CAgentServer::IsReserveCloseClient(DWORD dwClient)
{
    std::vector<DWORD>::iterator pos = std::find(m_vecCloseList.begin(), m_vecCloseList.end(), dwClient);        
    if (pos == m_vecCloseList.end())
        return false;
    else 
        return true;
}

F_SERVER_INFO* CAgentServer::GetFieldServerInfo(DWORD ServerID, DWORD Channel)
{
    if (ServerID < FIELDSERVER_MAX && Channel < MAX_CHANNEL_NUMBER)
        return &m_FieldServerInfo[Channel][ServerID];
    else
        return NULL;        
}

void CAgentServer::ClearBlockIp()
{
    if (m_pAddressChecker)
        m_pAddressChecker->ClearIpData();
}

Country::SCOUNTRY_INFO CAgentServer::GetCountry ( DWORD dwDbNum ) const
{
	if ( false == m_pClientManager )
		return Country::SCOUNTRY_INFO();

	DWORD dwClient = m_pClientManager->GetClientID( dwDbNum );

	return m_pClientManager->GetCountryInfo( dwClient );
}

net::EMSERVERTYPE CAgentServer::GetServerType()
{
	return m_pConfig->GetServerType();
}

std::string CAgentServer::GetIP()
{
	return std::string(m_pConfig->GetServerIP());
}

int	CAgentServer::GetPort()
{
	return m_pConfig->GetServicePort();
}

const bool CAgentServer::IsSubServer ()
{
	return m_pConfig->IsSubServer();
}

const bool CAgentServer::IsAgentClientSlot ( DWORD _dwClientSlotID )
{
	return m_pClientManager->IsAgentAgentSlot( _dwClientSlotID );
}

const bool CAgentServer::IsFieldClientSlot ( DWORD _dwClientSlotID )
{
	return m_pClientManager->IsAgentFieldSlot( _dwClientSlotID );
}

const DWORD CAgentServer::GetServerGroup_ClientSlotID ( DWORD _dwClientSlotID )
{
	return m_pClientManager->GetServerGroup( _dwClientSlotID );
}

const DWORD CAgentServer::GetClientSlotID_ServerGroup ( DWORD _dwServerGroup )
{
	return m_pClientManager->GetClientID_ServerGroup( _dwServerGroup );
}

const DWORD CAgentServer::GetWorldBattleServerNumberInfo ()
{
	return m_pConfig->GetWorldBattleServerNumberInfo();
}

void CAgentServer::AddGameDbJob(db::DbActionPtr spJob)
{    
    m_pDbManager->AddGameJob(spJob, m_pAdoManager);
}

void CAgentServer::AddGameAdoJob(db::DbActionPtr spJob)
{
    if (m_pAdoManager)
    {
        m_pAdoManager->AddGameJob(spJob, m_pAdoManager);
        return;
    }

    if (spJob)
    {
        sc::writeLogError(
            sc::string::format(
                "CAgentServer::AddGameAdoJob m_pAdoManager NULL %1%",
                typeid(*spJob).name()));
    }
}

std::string CAgentServer::GetClientIP( DWORD dwClient ) const
{
    return m_pClientManager->GetClientIP(dwClient);
}

void CAgentServer::ExecuteReservedTask()
{
	if ( m_pGLAgentServer )
	{
		m_pGLAgentServer->ExecuteReservedTask();
	}
}

void CAgentServer::EndRegularScheduleThread()
{

	TRACE("[RegularScheduleProc End]\n");

	CloseHandle(m_hRegularScheduleThread);
	m_hRegularScheduleThread = NULL;
}

DWORD CAgentServer::GetRandomFieldSlot()
{
	sc::writeLogInfo( "CAgentServer::GetRandomFieldSlot start find field slot" );

	WORD LoopBreak = 0;
	DWORD SlotNum = net::INVALID_SLOT;
	while ( true )
	{
		for ( size_t i = 0; i < MAX_CHANNEL_NUMBER; ++i )
		{
			for (size_t j = 0; j < FIELDSERVER_MAX; ++j )
			{
				SlotNum = m_FieldServerInfo[i][j].NetSlotNum;
				if ( SlotNum != net::INVALID_SLOT && m_pClientManager->IsOnline( SlotNum ) )
				{
					if ( sc::Random::getInstance().IsSucceed( 10.0f ) )
					{
						sc::writeLogInfo( sc::string::format( "CAgentServer::GetRandomFieldSlot find field slot %1%", SlotNum ) );
						return SlotNum;
					}
				}

				if ( ++LoopBreak > 30000 )
				{
					return net::INVALID_SLOT;
				}
			}
		}
	}
}
