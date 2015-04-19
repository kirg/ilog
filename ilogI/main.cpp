#include <windows.h>
#include <shlobj.h>
#include "resource.h"
#pragma comment(linker,"/align:4096")


LRESULT CALLBACK dlgproc(HWND hW,UINT msg,WPARAM wp,LPARAM lp);

#pragma comment(linker,"/entry:main")
void main(){
	ExitProcess(DialogBoxParam(0,(char *)IDD_dlg,0,(DLGPROC)dlgproc,0));
}

#define maxPath 300
int CreateLink(char *link,char *exe,char *arg,char *dir,WORD hotkey,
					char *icon,int icon_n,char *description,int showWindow){
  IShellLink* isl;
  IPersistFile* ipf;
  WORD wpath[maxPath];
  int result=0;

  CoInitialize(0);
  if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink,0,CLSCTX_INPROC_SERVER,IID_IShellLink,(void **)&isl))){
    if (SUCCEEDED(isl->QueryInterface(IID_IPersistFile,(void **)&ipf))){
		  if (SUCCEEDED(isl->SetPath(exe))){
		  	if (arg)
		  		isl->SetArguments(arg);
		  	if (dir)
		  		isl->SetWorkingDirectory(dir);
		  	if (hotkey)
		  		isl->SetHotkey(hotkey);
		  	if (icon)
		  		isl->SetIconLocation(icon,icon_n);
		  	if (description)
		  		isl->SetDescription(description);
		  	if (showWindow)
		  		isl->SetShowCmd(showWindow);
		  	result=1;
		  }
		  MultiByteToWideChar(CP_ACP,0,link,-1,wpath,maxPath);
		  ipf->Save(wpath,1);
		  ipf->Release();
    }
    isl->Release();
  }
  CoUninitialize();
  return result;
}

int createDirectory(char *dir){
  /*create directory, 'dir', recursively, and set current directory as 'dir'*/
	register int i;
	int j;
	j=*((unsigned long *)dir) & 0x00ffffff;
	if (!SetCurrentDirectory((char *)&j))
		return 0;
	dir[lstrlen(dir)+1]=0;/* double null terminate */
	for(i=j=3;dir[j];j=++i){
		while(dir[++i]!='\\' && dir[i]);dir[i]=0;
		if (!SetCurrentDirectory(dir+j)){
			CreateDirectory(dir+j,0);
			if (!SetCurrentDirectory(dir+j))
				return 0;
		}
	}
	return i;
}

#define ilog    "ilog.exe"
#define ilogV   "ilogV.exe"
#define ReadMe	"ReadMe.txt"

#define iloglnk	"ilogViewer_v0.8.3.lnk"
#define ilogdes	"Internet Usage Monitor"

#define reg_zyx	"SOFTWARE\\zyxWare"
#define prog	  "ilog"
#define progID	0x57010719

#define reg_app	"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\ilog.exe"
#define reg_mwc	"Software\\Microsoft\\Windows\\CurrentVersion"

#define maxPath 300
BROWSEINFO bi;
char path[maxPath];
char path0[maxPath];
HKEY hK;
HRSRC hR;
HANDLE hF;
unsigned long l;
char *buf;

LRESULT CALLBACK dlgproc(HWND hW,UINT msg,WPARAM wp,LPARAM lp){
	switch(msg){
	case WM_NCHITTEST:
		SetWindowLong(hW,DWL_MSGRESULT,HTCAPTION);
		return 1;
	case WM_COMMAND:
		switch(LOWORD(wp)){
		case IDC_open:
			RtlZeroMemory(&bi,sizeof(BROWSEINFO));
			bi.hwndOwner=hW;
			bi.ulFlags=BIF_RETURNONLYFSDIRS;
			bi.lpszTitle="please locate the folder where you want to install ilog ...";
			if (wp=(WPARAM)SHBrowseForFolder(&bi)){
				SHGetPathFromIDList((LPITEMIDLIST)wp,path);
				if (path[(lp=lstrlen(path))-1]!='\\')
					*((unsigned short *)(path+lp++))='\\';
				lstrcpyn(path+lp,"ilog\\",maxPath-lp);
				SendMessage(GetDlgItem(hW,IDC_path),WM_SETTEXT,0,(LPARAM)path);
			}
			break;
		case IDOK:
			lp=SendMessage(GetDlgItem(hW,IDC_path),WM_GETTEXT,maxPath,(LPARAM)path0);
			lstrcpy(path,path0);
			if (!createDirectory(path))/*create install dir*/
				goto error;
			if (path0[lp-1]!='\\')
				*((unsigned short *)(path0+lp++))='\\';
			lstrcpyn(path0+lp,ilogV,maxPath-lp);/*now contains the full path to ilogV.exe*/
			if (hR=FindResource(0,(char *)IDR_i,"i")){
				l=SizeofResource(0,hR);
				buf=(char *)LockResource(LoadResource(0,hR));
				if (hF=CreateFile(ilog,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0)){
					l=WriteFile(hF,buf,l,&l,0);
					CloseHandle(hF);
				}
			}
			if (!l || !hF){
error:
				MessageBox(hW,	"the installer encountered an error while creating \"ilog.exe\"\n"
								"please make sure that the specified path is valid and that\n"
								"an other version of \"ilog\" is not currently running from it.","ilogInstaller error!",MB_ICONERROR);
				goto reinit;
			}
			ShowWindow(hW,0);
			if (hR=FindResource(0,(char *)IDR_v,"v")){
				l=SizeofResource(0,hR);
				buf=(char *)LockResource(LoadResource(0,hR));
				if (hF=CreateFile(ilogV,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0)){
					l=WriteFile(hF,buf,l,&l,0);
					CloseHandle(hF);
				}
				if (!l || !hF){
					ShowWindow(hW,1);
					goto error;
				}
			}

			if (hR=FindResource(0,(char *)IDR_r,"r")){
				l=SizeofResource(0,hR);
				buf=(char *)LockResource(LoadResource(0,hR));
        if (hF=CreateFile(ReadMe,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0)){
					l=WriteFile(hF,buf,l,&l,0);
					CloseHandle(hF);
				}
			}

			lstrcat(lstrcpyn(path,path0,lstrlen(path0)-8/*-lstrlen(ilogV)+1*/),ilog);
			ShellExecute(0,0,path,0,0,0);/*run ilog*/

			lstrcat(lstrcpyn(path,path0,lstrlen(path0)-8/*-lstrlen(ilogV)+1*/),ReadMe);
			ShellExecute(0,0,path,0,0,SW_SHOWNORMAL);/*show "ReadMe.txt"*/

			if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,reg_zyx,0,0,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,0,(HKEY *)&hK,0)==ERROR_SUCCESS){
				l=progID;
				RegSetValueEx((HKEY)hK,prog,0,REG_DWORD,(unsigned char *)&l,sizeof(DWORD));
				RegCloseKey((HKEY)hK);
			}

			if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,reg_app,0,0,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,0,(HKEY *)&hK,0)==ERROR_SUCCESS){
				RegSetValueEx((HKEY)hK,0,0,REG_SZ,(unsigned char *)path0,lstrlen(path0)+1);
				RegCloseKey((HKEY)hK);
			}

			if (SendMessage(GetDlgItem(hW,IDC_desktop),BM_GETCHECK,0,0)==BST_CHECKED){
				SHGetSpecialFolderLocation(0,CSIDL_DESKTOPDIRECTORY,(LPITEMIDLIST *)&lp);
				SHGetPathFromIDList((LPITEMIDLIST)lp,path);
				if (SetCurrentDirectory(path))/*cur dir is desktop folder*/
					CreateLink(iloglnk,path0,0,0,0,path0,0,ilogdes,0);
			}
			if (SendMessage(GetDlgItem(hW,IDC_startmenu),BM_GETCHECK,0,0)==BST_CHECKED){
				SHGetSpecialFolderLocation(0,CSIDL_PROGRAMS,(LPITEMIDLIST *)&lp);
				SHGetPathFromIDList((LPITEMIDLIST)lp,path);
				if (SetCurrentDirectory(path))
					if (SetCurrentDirectory("_zyxWare_")
						|| (CreateDirectory("_zyxWare_",0)
							&& SetCurrentDirectory("_zyxWare_")))
						/*creates & sets as cur dir, ilog program group*/
						CreateLink(iloglnk,path0,0,0,0,path0,0,ilogdes,0);
			}
			Sleep(1000);
			MessageBeep(~0);/*the beep of success*/
			if (SendMessage(GetDlgItem(hW,IDC_run),BM_GETCHECK,0,0)==BST_CHECKED)
				ShellExecute(0,0,path0,0,0,0);/*run ilogV*/
			else
				MessageBox(0,"\"ilog\" installation was completed successfully!","ilogInstaller done!",MB_ICONINFORMATION);
		case IDC_cancel:
			EndDialog(hW,0);
			/*break;*/
		}
		break;
	case WM_INITDIALOG:
		SetWindowText(hW,"ilogInstaller_v0.0.9");
		SendMessage(GetDlgItem(hW,IDC_open),BM_SETIMAGE,IMAGE_ICON,(LPARAM)LoadIcon(GetModuleHandle(0),(char *)IDI_open));
		SendMessage(GetDlgItem(hW,IDC_desktop),BM_SETCHECK,BST_CHECKED,0);
		SendMessage(GetDlgItem(hW,IDC_startmenu),BM_SETCHECK,BST_CHECKED,0);
		SendMessage(GetDlgItem(hW,IDC_run),BM_SETCHECK,BST_CHECKED,0);
		SendMessage(GetDlgItem(hW,IDC_path),WM_SETTEXT,0,(LPARAM)path);
reinit:
/*HKEY_USERS\.DEFAULT\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders
FindExecutable
ShellExecuteEx		
		*/
		lstrcpy(path,"C:\\Program Files\\");/*the default value*/
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,reg_mwc,0,KEY_READ,&hK)==ERROR_SUCCESS){
			lp=maxPath;
			RegQueryValueEx(hK,"ProgramFilesDir",0,0,(unsigned char *)path,(DWORD *)&lp);
			RegCloseKey(hK);
    }
		lp=lstrlen(path);
		/*SHGetSpecialFolderLocation(0,CSIDL_PROGRAMS,(LPITEMIDLIST *)&lp);*/
		/*SHGetPathFromIDList((LPITEMIDLIST)lp,path);*/
		if (path[(lp=lstrlen(path))-1]!='\\')
			*((unsigned short *)(path+lp++))='\\';
		lstrcpyn(path+lp,"_zyxWare_\\ilog\\",maxPath-lp);
		SendMessage(GetDlgItem(hW,IDC_path),WM_SETTEXT,0,(LPARAM)path);
		break;
	}
	return 0;
}