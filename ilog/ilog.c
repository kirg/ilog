#include <windows.h>
#include <ras.h>
/*#include <wininet.h>*/
#pragma comment(linker,"/align:4096")
#pragma comment(linker,"/nodefaultlib")
#pragma comment(linker,"/merge:.rdata=.text")

#define defaultLogInterval	16000
#define logFileName			    "ilog.log"

#define wndname				      "ilogWnd"
#define oldwndname			    "ilog"
#define progName			      "ilog"

#define regRunServices		  "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices"

#define evDisconnect	      0
#define evConnect		        1
#define evPowerOn		        65536
#define evPowerOff		      65537

int previousStatus;
int currentStatus;

void logEvent(unsigned int status,unsigned int goback){
	SYSTEMTIME time;
	char logStr[256];
	char *ev;
	HANDLE hF;

	if (status==evPowerOn){
		ev+=GetModuleFileName(0,ev=logStr,256);
		while(*(--ev)!='\\');
		*(++ev)=0;
		SetCurrentDirectory(logStr);
		/*GetSystemDirectory(logStr,256);*/
		/*SetCurrentDirectory(logStr);*/
	}
	if (!(hF=CreateFile(logFileName,GENERIC_WRITE,FILE_SHARE_READ,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0)))
    return;
	switch(status){
	case evDisconnect:
		ev="OFF";
		break;
	case evConnect:
		ev="ON!";
		break;
	case evPowerOn:
		ev="hi!";
		break;
	case evPowerOff:
		ev="bye";
		/*break;*/
	}
	SetFilePointer(hF,-32*goback,0,FILE_END);
	GetLocalTime(&time);
	goback=wsprintf(logStr,"[%.2u/%.2u/%.4u] %.2u:%.2u:%.2u  %s    \r\n",time.wDay,time.wMonth,time.wYear,time.wHour,time.wMinute,time.wSecond,ev);
	WriteFile(hF,logStr,goback,&goback,0);
	CloseHandle(hF);
	if (status==evPowerOn && RegCreateKeyEx(HKEY_LOCAL_MACHINE,regRunServices,0,0,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,0,(HKEY *)&hF,0)==ERROR_SUCCESS){
		status=GetModuleFileName(0,logStr,256);
		RegSetValueEx((HKEY)hF,progName,0,REG_SZ,logStr,status);
		RegCloseKey((HKEY)hF);
	}
}


WNDCLASS wc;
unsigned int interval=defaultLogInterval;

LRESULT CALLBACK wndproc(HWND,UINT,WPARAM,LPARAM);

#pragma comment(linker,"/entry:main")
int main(){
	MSG msg;
	HWND hW;
	DWORD pid;
	HANDLE hP;
	
	if (hW=FindWindow(oldwndname,0)){
		GetWindowThreadProcessId(hW,&pid);
		if (hP=OpenProcess(PROCESS_TERMINATE,0,pid)){
			TerminateProcess(hP,0);
			CloseHandle(hP);
		}
	}

	if (FindWindow(wndname,0))
		return 0;
	wc.lpfnWndProc=wndproc;
	wc.lpszClassName=wndname;
	RegisterClass(&wc);
	SetTimer(CreateWindow(wndname,0,0,0,0,0,0,0,0,0,0),1,interval,0);
	(*GetProcAddress(GetModuleHandle("kernel32.dll"),"RegisterServiceProcess"))(0,1);
	previousStatus=currentStatus=0;
	SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
	logEvent(evPowerOn,0);
	while(GetMessage(&msg,0,0,0))
		DispatchMessage(&msg);
	ExitProcess(0);
	return 0;
}

RASCONN			  rasConn;
RASCONNSTATUS	rasConnStatus;
int				    num;

LRESULT CALLBACK wndproc(HWND hWi,UINT m,WPARAM wp,LPARAM lp){
	switch(m){
		case WM_TIMER:
			rasConn.dwSize=m=sizeof(RASCONN);
			rasConnStatus.dwSize=sizeof(RASCONNSTATUS);
			currentStatus=!RasEnumConnections(&rasConn,&m,&num) && num &&
					!RasGetConnectStatus(rasConn.hrasconn,&rasConnStatus) &&
						(rasConnStatus.rasconnstate==RASCS_Connected);
//			currentStatus=INTERNET_CONNECTION_MODEM|INTERNET_CONNECTION_LAN;
//			if (!InternetGetConnectedState(&currentStatus,0))
//				currentStatus=0;
			if (!currentStatus && previousStatus)
				logEvent(evDisconnect,1);
			if (currentStatus && !previousStatus){
				logEvent(evConnect,0);
				logEvent(evPowerOff,0);
			}
			if (currentStatus && previousStatus)
				logEvent(evPowerOff,1);
			previousStatus=currentStatus;
			break;
		case WM_ENDSESSION:
			if (wp && lp!=ENDSESSION_LOGOFF)
				logEvent(evPowerOff,0);
			/*break;*/
	}
	return DefWindowProc(hWi,m,wp,lp);
}