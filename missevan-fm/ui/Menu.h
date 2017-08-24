
#ifndef _SNOW_CUTE_FRAME_MENU_H_
#define _SNOW_CUTE_FRAME_MENU_H_ 1

#include "../base/common.h"

//namespace view{
//	namespace frame{

		class CMenu{
		public:
			CMenu();
			virtual ~CMenu();

			bool Create(bool autodelete_ = true);
			bool CreatePopup(bool autodelete_ = true);

			bool Append(unsigned short id, LPCWSTR text, UINT flags = MF_STRING, CMenu *pMenu = NULL);
			bool Insert(UINT position, unsigned short id, LPCWSTR text, UINT flags = MF_STRING | MF_BYPOSITION, CMenu *pMenu = NULL);
			bool Modify(UINT position, unsigned short id, LPCWSTR text, UINT flags = MF_STRING | MF_BYPOSITION, CMenu *pMenu = NULL);
			bool Delete(UINT position, bool byposition = true);

			bool Remove();

			bool EnableItem(UINT id, bool enable_ = true, bool byposition = true);
			bool CheckItem(UINT id, bool enable_ = true, bool byposition = true);
			bool SetDefaultItem(UINT item, bool byposition = true);

			// by command
			bool Insert(unsigned short id, LPCWSTR text, UINT flags = MF_STRING | MF_BYCOMMAND, CMenu *pMenu = NULL);
			bool Modify(unsigned short id, LPCWSTR text, UINT flags = MF_STRING | MF_BYCOMMAND, CMenu *pMenu = NULL);

			void Show(HWND hWnd, UINT flags = TPM_LEFTALIGN | TPM_RIGHTBUTTON, POINT *pt = NULL);

			HMENU GetMenu() {
				return m_hMenu;
			}

		protected:
			HMENU m_hMenu;
			bool m_autodelete;
		};

//	};
//};

#endif