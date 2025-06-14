//	마켓 윈도우
//
//	최초 작성자 : 성기엽
//	이후 수정자 : 
//	로그
//		[2003.12.6]
//			@ 작성
//

#pragma	once

#include "../Util/UIWindowEx.h"
#include "../../InnerInterface.h"

class CInnerInterface;
struct SITEMCUSTOM;
class   CRnPageBase;
class	CRnPageInven;
class GLGaeaClient;

class CTradeInventoryWindow : public CUIWindowEx
{
protected:
	enum
	{
		TRADEINVENTORY_MONEY_BUTTON = ET_CONTROL_NEXT,
		TRADEINVENTORY_PLUS_MONEY_BUTTON,
		TRADEINVENTORY_MINUS_MONEY_BUTTON,
		TRADEINVENTORY_PAGE,
	};

public:
	CTradeInventoryWindow(GLGaeaClient* pGaeaClient, CInnerInterface* pInterface, EngineDeviceMan* pEngineDevice);
	virtual	~CTradeInventoryWindow();

protected:
    GLGaeaClient* m_pGaeaClient;

public:
	void	CreateSubControl();

public:
	virtual	void Update( int x, int y, BYTE LB, BYTE MB, BYTE RB, int nScroll, float fElapsedTime, BOOL bFirstControl );
	virtual	void TranslateUIMessage( UIGUID ControlID, DWORD dwMsg );
	virtual HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICEQ pd3dDevice );

public:
	void	SetOPENER( bool bOPENER );

private:
	void	SetMoney ( LONGLONG Money );
	bool	IsTurnITEM (const SITEMCUSTOM& sItemCustom);

public:
	void	SetOneViewSlot( const int& nONE_VIEW_SLOT );
	const int& GetOneViewSlot() const;

private:
	CRnPageBase* m_pCurrent;
	CRnPageInven*		m_pPage;

private:
	CBasicTextBox*		m_pMoneyTextBox;

	bool	m_bTRADE_MODE;
	bool	m_bOPENER;

	int		m_nONE_VIEW_SLOT;

};

class MyTradeInventoryWindow : public ITradeInventoryWindow, private CTradeInventoryWindow
{
public:
    MyTradeInventoryWindow( GLGaeaClient* pGaeaClient, CInnerInterface* pInterface, EngineDeviceMan* pEngineDevice );
    virtual ~MyTradeInventoryWindow()   {};

public:
    virtual void CreateUIWindowAndRegisterOwnership();

    virtual const UIRECT& GetUIWindowGlobalPos();
    virtual void SetUIWindowGlobalPos( const D3DXVECTOR2& vPos );
    virtual void SetOneViewSlot( const int& nONE_VIEW_SLOT );
    virtual void SetOpener( bool bOPENER );
};
