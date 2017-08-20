
#include "stdafx.h"

#include <Windows.h>

#include "Menu.h"
#include "../base/global.h"

//namespace view{
//	namespace frame{
		CMenu::CMenu()
			: m_hMenu(NULL)
			, m_autodelete(true)
		{
		}

		CMenu::~CMenu()
		{
			if (m_autodelete) Remove();
		}

		bool CMenu::Create(bool autodelete_)
		{
			if (m_autodelete) Remove();

			m_autodelete = autodelete_;
			m_hMenu = ::CreateMenu();

			return (m_hMenu != NULL);
		}

		bool CMenu::CreatePopup(bool autodelete_)
		{
			if (m_autodelete) Remove();

			m_autodelete = autodelete_;
			m_hMenu = ::CreatePopupMenu();

			return (m_hMenu != NULL);
		}

		bool CMenu::Append(unsigned short id, LPCWSTR text, UINT flags, CMenu *pMenu)
		{
			return static_cast<bool>(AppendMenuW(m_hMenu, flags | (pMenu ? MF_POPUP : NULL), (pMenu ? (UINT_PTR)pMenu->m_hMenu : (UINT_PTR)id), text));
		}

		bool CMenu::Insert(UINT position, unsigned short id, LPCWSTR text, UINT flags, CMenu *pMenu)
		{
			return static_cast<bool>(InsertMenuW(m_hMenu, position, flags | (pMenu ? MF_POPUP : NULL), (pMenu ? (UINT_PTR)pMenu->m_hMenu : (UINT_PTR)id), text));
		}

		bool CMenu::Modify(UINT position, unsigned short id, LPCWSTR text, UINT flags, CMenu *pMenu)
		{
			return static_cast<bool>(ModifyMenuW(m_hMenu, position, flags | (pMenu ? MF_POPUP : NULL), (pMenu ? (UINT_PTR)pMenu->m_hMenu : (UINT_PTR)id), text));
		}

		bool CMenu::Delete(UINT position, bool byposition)
		{
			return static_cast<bool>(DeleteMenu(m_hMenu, position, byposition ? MF_BYPOSITION : MF_BYCOMMAND));
		}

		// by command
		bool CMenu::Insert(unsigned short id, LPCWSTR text, UINT flags, CMenu *pMenu)
		{
			return static_cast<bool>(InsertMenuW(m_hMenu, id, flags | (pMenu ? MF_POPUP : NULL), (pMenu ? (UINT_PTR)pMenu->m_hMenu : (UINT_PTR)id), text));
		}

		bool CMenu::Modify(unsigned short id, LPCWSTR text, UINT flags, CMenu *pMenu)
		{
			return static_cast<bool>(ModifyMenuW(m_hMenu, id, flags | (pMenu ? MF_POPUP : NULL), (pMenu ? (UINT_PTR)pMenu->m_hMenu : (UINT_PTR)id), text));
		}

		bool CMenu::Remove()
		{
			if (m_hMenu) {
				return static_cast<bool>(::DestroyMenu(m_hMenu));
			}
			return true;
		}

		bool CMenu::EnableItem(UINT id, bool enable_, bool byposition)
		{
			BOOL uEnable = enable_ ? MF_ENABLED : MF_GRAYED;
			if (byposition) {
				uEnable |= MF_BYPOSITION;
			} else {
				uEnable |= MF_BYCOMMAND;
			}
			return static_cast<bool>(::EnableMenuItem(m_hMenu, id, uEnable));
		}

		bool CMenu::SetDefaultItem(UINT item, bool byposition)
		{
			return static_cast<bool>(::SetMenuDefaultItem(m_hMenu, item, byposition ? TRUE : FALSE));
		}

		void CMenu::Show(HWND hWnd, UINT flags, POINT *pt)
		{
			POINT pos = {0};
			if (pt) {
				memcpy(&pos, pt, sizeof(POINT));
			} else {
				::GetCursorPos(&pos);
			}

			::SetForegroundWindow(hWnd);
			::TrackPopupMenu(m_hMenu, flags, pos.x, pos.y, 0, hWnd, NULL);
		}

//	};
//};
