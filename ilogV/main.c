#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#pragma comment(linker,"/align:4096")
#pragma comment(linker,"/nodefaultlib")



#define logFileName			    "ilog.log"
#define iniFileName			    "ilog.ini"

#define def_logInterval		  16 /*seconds*/

#define def_pulseRate		    180
#define def_connTime		    60
#define def_callCharge		  120
#define def_callCharge_str	"1.20"

#define bufSize				      32
#define sizeOfLog			      32

#define WM_MAINDLGMOVE		  WM_USER+142
#define WM_CFGDONE			    WM_USER+143
#define maxPath				      256

char  path[maxPath];
char  buf[bufSize];

LRESULT CALLBACK dlgproc(HWND hW,UINT msg,WPARAM wp,LPARAM lp);

#pragma comment(linker,"/entry:main")
void main(){
	register int i;
	InitCommonControls();
	i=GetModuleFileName(0,path,maxPath);
	while(path[--i]!='\\');path[++i]=0;
	SetCurrentDirectory(path);
	lstrcpyn(path+i,iniFileName,maxPath-i);
	ExitProcess(DialogBoxParam(0,(char *)IDD_dlg,0,dlgproc,0));
}


unsigned int _atoi(const char *a){
	register int i=0;
	while(*a){
		i*=10;
		i+=*(a++)-'0';
	}
	return i;
}

unsigned long readlog(HANDLE hF,SYSTEMTIME *st){
	/*char buf[sizeOfLog];/*globalised*/
	ReadFile(hF,buf,sizeOfLog,(DWORD *)&hF,0);
	if (!hF)
		return 0;
	/* "[18/03/2001] 08:26:30  hi!      " -> "[18*03*2001* 08*26*30* hi!*     " */
	buf[3]=buf[6]=buf[11]=buf[15]=buf[18]=buf[21]=buf[26]=0;
	st->wDay=_atoi(buf+1);
	st->wMonth=_atoi(buf+4);
	st->wYear=_atoi(buf+7);
	st->wHour=_atoi(buf+13);
	st->wMinute=_atoi(buf+16);
	st->wSecond=_atoi(buf+19);
	st->wDayOfWeek=0;
	st->wMilliseconds=0;
	return *((unsigned long *)(buf+22));
}

unsigned long getMonthOffs(HANDLE hF,int month,int year)
{
	/*char buf[sizeOfLog];/*globalised*/
	int top,bot,mid;
	int lyear,lmonth;

	top=0;bot=(GetFileSize(hF,0)/sizeOfLog/*>>5*/)-1;
	while(top<=bot){
		mid=(top+bot)/2;
		SetFilePointer(hF,mid*sizeOfLog/*<<5*/,0,FILE_BEGIN);
		ReadFile(hF,buf,sizeOfLog,(DWORD *)&lmonth,0);
		buf[6]=buf[11]=0;
		lmonth=_atoi(buf+4);
		lyear=_atoi(buf+7);
		if (month==lmonth && year==lyear)
			goto foundMonthLog;
		if (year<lyear || (month<lmonth && year==lyear))
			bot=mid-1;
		else
			top=mid+1;
	}
	return ~0;/*not found*/
foundMonthLog:
  do{/*now search back for the first day of the month*/
		if (!(mid--))
		{
			SetFilePointer(hF,-sizeOfLog,0,FILE_CURRENT);
			return 0;
		}
		SetFilePointer(hF,-2*sizeOfLog,0,FILE_CURRENT);
 		ReadFile(hF,buf,sizeOfLog,(DWORD *)&lmonth,0);
		buf[6]=buf[11]=0;
		lmonth=_atoi(buf+4);
		lyear=_atoi(buf+7);
	}while(month==lmonth && year==lyear);
	return (mid+1)<<5;/*return offset*/
}

unsigned long timediff(SYSTEMTIME *st0,SYSTEMTIME *st1){	
	FILETIME t0,t1;
	SystemTimeToFileTime(st0,&t0);
	SystemTimeToFileTime(st1,&t1);
	return (33554432*(t1.dwHighDateTime-t0.dwHighDateTime)+(t1.dwLowDateTime>>7)-(t0.dwLowDateTime>>7))/78125;
	/*return (unsigned long)((((ULARGE_INTEGER *)&t1)->QuadPart-((ULARGE_INTEGER *)&t0)->QuadPart)/10000000);*/
	/*return ((33554432*(t1.dwHighDateTime-t0.dwHighDateTime)/78125)+(((t1.dwLowDateTime>>7)-(t0.dwLowDateTime>>7))/78125));*/
}

char  month[][10]={
      "January","February","March",
      "April","May","June","July",
      "August","September","October",
      "November","December"
      };

int pulseRate=180,callCharge=120,connTime=60;
int curDay,curMonth,curYear;
int curDay_j;/*contains the index of the current day in the month log list view*/

unsigned long curMonthOffs;/*contains offset for the first day of the current month*/
unsigned long dayOffs[32];/*contains offsets for each day of the current month*/


int displayDayLog(HWND hW){
	/*char buf[sizeOfLog];/*globalised*/
	HANDLE  			hF;
	HWND			    hWt;
	LV_ITEM		  	lvi;
	SYSTEMTIME		t0,t1;
	unsigned int	i,j;
	register int	k;
	int				    secs,calls;

	if (!curDay || curDay>31){
		SendMessage(GetDlgItem(hW,IDC_dayLogTitle),WM_SETTEXT,0,0);
		SendMessage(GetDlgItem(hW,IDC_dayLogTotal),WM_SETTEXT,0,0);
		ListView_DeleteAllItems(hWt=GetDlgItem(hW,IDC_dayLog));
		ListView_SetColumnWidth(hWt,1,80);
		ListView_SetColumnWidth(hWt,2,60);
		return ~0;
	}

	if (hF=CreateFile(logFileName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0)){
		hWt=GetDlgItem(hW,IDC_dayLog);
		ListView_DeleteAllItems(hWt);

		j=secs=calls=0;

		SetFilePointer(hF,dayOffs[curDay],0,FILE_BEGIN);

		lvi.mask=LVIF_TEXT|LVIF_PARAM;
		lvi.iSubItem=0;

		if (~SetFilePointer(hF,-sizeOfLog,0,FILE_CURRENT) && readlog(hF,&t1)=='!NO '){
			/* only if the 'jump back' was successful; ie if not at the beginning of the file */
			readlog(hF,&t0);
			t1.wYear=t0.wYear;t1.wMonth=t0.wMonth;t1.wDay=t0.wDay;
			t1.wHour=t1.wMinute=t1.wSecond=0;
			secs=(k=timediff(&t1,&t0));
			calls=1+(k+connTime)/pulseRate;

			lvi.pszText="00:00:00";
			lvi.iItem=j;
			lvi.lParam=((j&0x1F)<<27)+((calls&0x03FF)<<17)+(secs&0x1FFFF);
			ListView_InsertItem(hWt,&lvi);
			wsprintf(buf,"%.2u:%.2u:%.2u",k/3600,(k/60)%60,k%60);
			ListView_SetItemText(hWt,j,1,buf);
			wsprintf(buf,"%u",calls);
			ListView_SetItemText(hWt,j,2,buf);
			++j;
		}
		else
			secs=calls=0;

		while((i=readlog(hF,&t0)) && t0.wYear==curYear && t0.wMonth==curMonth && t0.wDay==curDay){
			if (i=='!NO '){
				/*i=*/readlog(hF,&t1);
				if (t1.wDay==curDay && t1.wMonth==curMonth && t1.wYear==curYear){
					secs+=(k=timediff(&t0,&t1));
					calls+=1+(k+connTime)/pulseRate;
				}
				else{
					t1.wYear=curYear;t1.wMonth=curMonth;t1.wDay=curDay;
					t1.wHour=23;t1.wMinute=t1.wSecond=59;
					secs+=(k=timediff(&t0,&t1)+1);
					calls+=1+(k+connTime)/pulseRate;
					SetFilePointer(hF,-sizeOfLog,0,FILE_CURRENT);
				}
				wsprintf(buf,"%.2u:%.2u:%.2u",t0.wHour,t0.wMinute,t0.wSecond);
				lvi.pszText=buf;
				lvi.iItem=j;
				lvi.lParam=((j&0x1F)<<27)+(((1+(k+connTime)/pulseRate)&0x03FFF)<<13)+((k&0x1FFF0)>>4);
				ListView_InsertItem(hWt,&lvi);
				wsprintf(buf,"%.2u:%.2u:%.2u",k/3600,(k/60)%60,k%60);
				ListView_SetItemText(hWt,j,1,buf);
				wsprintf(buf,"%u",1+(k+connTime)/pulseRate);
				ListView_SetItemText(hWt,j,2,buf);
				++j;
			}
		}
		CloseHandle(hF);
	}
	wsprintf(buf,"usage on %.2u-%3.3s-%.4u",curDay,month[curMonth-1],curYear);
	SendMessage(GetDlgItem(hW,IDC_dayLogTitle),WM_SETTEXT,0,(LPARAM)buf);
	wsprintf(buf,"total %.2u:%.2u:%.2u (%u calls)",secs/3600,(secs/60)%60,secs%60,calls);
	SendMessage(GetDlgItem(hW,IDC_dayLogTotal),WM_SETTEXT,0,(LPARAM)buf);
	if ((secs=24-GetSystemMetrics(SM_CXVSCROLL))<0)
		secs=0;
	if (j<11){
		ListView_SetColumnWidth(hWt,1,80);
		ListView_SetColumnWidth(hWt,2,60);
	}
	else{
		ListView_SetColumnWidth(hWt,1,68+secs-secs/2);
		ListView_SetColumnWidth(hWt,2,48+secs/2);
	}
	return 0;
}

int displayMonthLog(HWND hW,BOOL changeOfMonth/*:TRUE->update current day selection*/){
	/*char buf[sizeOfLog];/*globalised*/
	HANDLE			hF;/*for the log file*/
	HWND			hWt;/*temp hWnd*/
	LV_COLUMN		lvc;
	LV_ITEM			lvi;
	SYSTEMTIME		t0,t1;
	unsigned int	i,j;
	register int	k;
	int				secs,calls,tsecs,tcalls;
	int				day;

	hWt=GetDlgItem(hW,IDC_monthLog);
	ListView_DeleteAllItems(hWt);
	j=tcalls=tsecs=secs=calls=day=0;
	if (curMonth>12)
		return ~0;
	if (hF=CreateFile(logFileName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0)){
		if (!curMonth && !curYear)/*==0*//*do initialisation*/{
			lvc.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
			lvc.fmt=LVCFMT_CENTER;
			/*hWt=GetDlgItem(hW,IDC_monthLog);*//*initialized, above*/
			lvc.cx=38;lvc.pszText="day";
			ListView_InsertColumn(hWt,lvc.iSubItem=0,&lvc);
			lvc.cx=85;lvc.pszText="duration";
			ListView_InsertColumn(hWt,lvc.iSubItem=1,&lvc);
			lvc.cx=61;lvc.pszText="calls";
			ListView_InsertColumn(hWt,lvc.iSubItem=2,&lvc);
			hWt=GetDlgItem(hW,IDC_dayLog);
			lvc.cx=68;lvc.pszText="start";
			ListView_InsertColumn(hWt,lvc.iSubItem=0,&lvc);
			lvc.cx=68;lvc.pszText="duration";
			ListView_InsertColumn(hWt,lvc.iSubItem=1,&lvc);
			lvc.cx=48;lvc.pszText="calls";
			ListView_InsertColumn(hWt,lvc.iSubItem=2,&lvc);
			hWt=GetDlgItem(hW,IDC_month);
			for(i=0;i<12;++i)
				SendMessage(hWt,CB_INSERTSTRING,-1,(LPARAM)month[i]);
			SendMessage(GetDlgItem(hW,IDC_yearSpin),UDM_SETRANGE,0,MAKELONG(9999,1900));
			/*
			SetFilePointer(hF,-sizeOfLog,0,FILE_END);
			readlog(hF,&t0);
			*//*use current day/month as default, instead of the last log*/
			GetLocalTime(&t0);
			curYear=t0.wYear;
			SendMessage(GetDlgItem(hW,IDC_yearSpin),UDM_SETPOS,0,MAKELONG(curYear,0));
			curMonth=t0.wMonth;
			SendMessage(GetDlgItem(hW,IDC_month),CB_SETCURSEL,curMonth-1,0);
			/*curDay=0;*//*initialized in WM_INITDIALOG*/
			hWt=GetDlgItem(hW,IDC_monthLog);
		}
		if (!~(curMonthOffs=getMonthOffs(hF,curMonth,curYear)))/*==~0->month log not found*/
			goto skipProcessing;
		lvi.mask=LVIF_TEXT|LVIF_PARAM;
		lvi.iSubItem=0;
		lvi.pszText=buf;
		do{
			i=readlog(hF,&t0);
			if (t0.wDay!=day || t0.wMonth!=curMonth || t0.wYear!=curYear || !i)/*change in day*/{
				if (i && t0.wMonth==curMonth && t0.wYear==curYear)/*end of day; save new offset*/
					dayOffs[t0.wDay]=SetFilePointer(hF,0,0,FILE_CURRENT)-sizeOfLog;
				tsecs+=secs;tcalls+=calls;
				if (day){
					/*
					lvi.mask=LVIF_TEXT|LVIF_PARAM;
					lvi.iSubItem=0;
					lvi.pszText=buf;
					*//*initialized before loop; remains unchanged within the loop*/
					wsprintf(buf,"%.2u",day);
					lvi.iItem=j;
					lvi.lParam=((day&0x1F)<<27)+((calls&0x03FFF)<<13)+((secs&0x1FFF0)>>4);
					ListView_InsertItem(hWt,&lvi);
					wsprintf(buf,"%.2u:%.2u:%.2u",secs/3600,(secs/60)%60,secs%60);
					ListView_SetItemText(hWt,j,1,buf);
					wsprintf(buf,"%u",calls);
					ListView_SetItemText(hWt,j,2,buf);
					++j;
				}
				if (~SetFilePointer(hF,-2*sizeOfLog,0,FILE_CURRENT)){
					if (readlog(hF,&t1)=='!NO '){
						t1.wYear=t0.wYear;t1.wMonth=t0.wMonth;t1.wDay=t0.wDay;
						t1.wHour=t1.wMinute=t1.wSecond=0;
						/*24hour online, illegal; will not appear in display*/
						/*t1.wHour=23;t1.wMinute=t1.wSecond=59;*/
						secs=(k=timediff(&t1,&t0));
						calls=1+(k+connTime)/pulseRate;
					}
					else
						secs=calls=0;
					SetFilePointer(hF,sizeOfLog,0,FILE_CURRENT);
				}
				else
					secs=calls=0;
				day=t0.wDay;
			}
			if (i=='!NO '){
				i=readlog(hF,&t1);
				if (t1.wDay==day && t1.wMonth==curMonth && t1.wYear==curYear){
					secs+=(k=timediff(&t0,&t1));
					calls+=1+(k+connTime)/pulseRate;
				}
				else{
					t1.wYear=t0.wYear;t1.wMonth=t0.wMonth;t1.wDay=t0.wDay;
					t1.wHour=23;t1.wMinute=t1.wSecond=59;
					secs+=(k=timediff(&t0,&t1)+1);
					calls+=1+(k+connTime)/pulseRate;
					SetFilePointer(hF,-sizeOfLog,0,FILE_CURRENT);
				}
			}
		}while(t0.wMonth==curMonth && t0.wYear==curYear && i && t0.wDay<32);
skipProcessing:
		CloseHandle(hF);
	}
/*	else
		return ~0;*/
	wsprintf(buf,"total for %s %.4u",month[curMonth-1],curYear);
	SendMessage(GetDlgItem(hW,IDC_monthTotalTitle),WM_SETTEXT,0,(LPARAM)buf);

	if ((secs=24-GetSystemMetrics(SM_CXVSCROLL))<0)
		secs=0;

	/*only 18 items fit into the log window*/
	if (j<19)/*no more than 18 -> use the entire title area*/{
		ListView_SetColumnWidth(hWt,1,97);
		ListView_SetColumnWidth(hWt,2,73);
	}
	else/*more than 18 -> allow space for scroll bar, depending on its size*/{
		ListView_SetColumnWidth(hWt,1,85+secs-secs/2);
		ListView_SetColumnWidth(hWt,2,61+secs/2);
	}
	if (j){
		wsprintf(buf,"%.2u:%.2u:%.2u",tsecs/3600,(tsecs/60)%60,tsecs%60);
		SendMessage(GetDlgItem(hW,IDC_totalTime),WM_SETTEXT,0,(LPARAM)buf);
		wsprintf(buf,"%u",tcalls);
		SendMessage(GetDlgItem(hW,IDC_totalCalls),WM_SETTEXT,0,(LPARAM)buf);
		wsprintf(buf,"%u.%.2u",callCharge*tcalls/100,(callCharge*tcalls)%100);
		SendMessage(GetDlgItem(hW,IDC_totalCharge),WM_SETTEXT,0,(LPARAM)buf);
		if (changeOfMonth)/*->current day must be changed*/{
			/*to enable use of arrow keys, initially, this must be done*/
			PostMessage(hWt,WM_KEYDOWN,curDay?VK_HOME:VK_END,0);
			ListView_GetItemText(hWt,curDay_j=curDay?0:j-1,0,buf,bufSize);
			ListView_GetItemText(hWt,curDay_j,0,buf,bufSize);
			curDay=_atoi(buf);
			displayDayLog(hW);
		}
		else
			displayDayLog(hW);
		ListView_SetItemState(hWt,curDay_j,LVIS_SELECTED,LVIS_SELECTED);
		PostMessage(hWt,LVM_ENSUREVISIBLE,curDay_j,0);
	}
	else{
		SendMessage(GetDlgItem(hW,IDC_totalTime),WM_SETTEXT,0,(LPARAM)"Not Available");
		SendMessage(GetDlgItem(hW,IDC_totalCalls),WM_SETTEXT,0,(LPARAM)"Not Available");
		SendMessage(GetDlgItem(hW,IDC_totalCharge),WM_SETTEXT,0,(LPARAM)"Not Available");
		j=curDay;/*save curDay value*/
		curDay=0;/*curDay=0 -> displayDayLog will clear the day log*/
		displayDayLog(hW);/*to clear the day log*/
		curDay=j;
	}
	return 0;
}

int CALLBACK compare(LPARAM lp1,LPARAM lp2,LPARAM lps) 
{
	switch(lps)/*sort by?*/{
	case 0:/*sort by month or start time*/
		return ((lp1&0xF8000000)>>1)-((lp2&0xF8000000)>>1);/*bits 27 to 31*/
	case 1:/*sort by duration*/
		return (lp1&0x0001FFF)-(lp2&0x0001FFF);/*bits 00 to 13*/
	default:/*sort by number of calls*/
		return (lp1&0x03FFF000)-(lp2&0x03FFF000);/*bits 14 to 26*/
	}
}

HWND hWc;
RECT r;
int l,_callCharge;

LRESULT CALLBACK config_wndproc(HWND hW,UINT msg,WPARAM wp,LPARAM lp){
	switch(msg){
	case WM_MAINDLGMOVE:
		GetWindowRect(hW,&r);
		r.left+=wp;r.right+=wp;
		r.top+=lp;r.bottom+=lp;
		MoveWindow(hW,r.left,r.top,r.right-r.left,r.bottom-r.top,1);
		break;
	case WM_COMMAND:
		switch(LOWORD(wp)){
		case IDOK:
			l=0;
      if (!SendMessage(GetDlgItem(hW,IDC_pulseRate),WM_GETTEXTLENGTH,0,0)){
				l=1;
				wsprintf(buf,"%u",pulseRate);
				SendMessage(GetDlgItem(hW,IDC_pulseRate),WM_SETTEXT,0,(LPARAM)buf);
			}
			if (!SendMessage(GetDlgItem(hW,IDC_callCharge),WM_GETTEXTLENGTH,0,0)){
				l=1;
				wsprintf(buf,"%u.%.2u",callCharge/100,callCharge%100);
				SendMessage(GetDlgItem(hW,IDC_callCharge),WM_SETTEXT,0,(LPARAM)buf);
			}
			if (!SendMessage(GetDlgItem(hW,IDC_connTime),WM_GETTEXTLENGTH,0,0)){
				l=1;
				wsprintf(buf,"%u",IDC_connTime);
				SendMessage(GetDlgItem(hW,IDC_connTime),WM_SETTEXT,0,(LPARAM)buf);
			}
			if (l){
				MessageBeep(~0);
				return 0;
			}

			l=SendMessage(GetDlgItem(hW,IDC_callCharge),WM_GETTEXT,bufSize,(LPARAM)buf);
			_callCharge=~0;
			while(l--){
				if (buf[l]=='.' && !~_callCharge){
					buf[l]=0;
					/*_callCharge=buf[l+1]_atoi(buf+l+1);*/
					_callCharge=0;
					if (buf[l+1]){
						_callCharge=(buf[l+1]-'0')*10;
						if (buf[l+2]){
							_callCharge+=buf[l+2]-'0';
							if (buf[l+3] && buf[l+3]>'4' && buf[l+3]<='9')
								++_callCharge;
						}
					}
				}
				else
					if (buf[l]<'0' || buf[l]>'9'){
						MessageBeep(~0);
						SendMessage(hW=GetDlgItem(hW,IDC_callCharge),EM_SETSEL,0,-1);
						SetFocus(hW);
						return 0;
					}
			}
			if (!(pulseRate=GetDlgItemInt(hW,IDC_pulseRate,0,0)))
				pulseRate=def_pulseRate;

			callCharge=_atoi(buf)*100+((~_callCharge)?_callCharge:0);
			connTime=GetDlgItemInt(hW,IDC_connTime,0,0);

			wsprintf(buf,"%u",pulseRate);
			WritePrivateProfileString("config","pulseRate",buf,path);
			wsprintf(buf,"%u.%.2u",callCharge/100,callCharge%100);
			WritePrivateProfileString("config","callCharge",buf,path);
			wsprintf(buf,"%u",connTime);
			WritePrivateProfileString("config","connTime",buf,path);
			connTime+=def_logInterval;/****************/
			PostQuitMessage(1/*change in config*/);
			break;
		case IDCANCEL:
			PostQuitMessage(0/*no change in config*/);
			/*break;*/
		}
		break;
	case WM_INITDIALOG:
		GetWindowRect(hW,&r);
		MoveWindow(hW,lp&0xffff,(lp&0xffff0000)>>16,r.right-r.left,r.bottom-r.top,1);
		wsprintf(buf,"%u",pulseRate);
		SendMessage(GetDlgItem(hW,IDC_pulseRate),WM_SETTEXT,0,(LPARAM)buf);
		wsprintf(buf,"%u.%.2u",callCharge/100,callCharge%100);
		SendMessage(GetDlgItem(hW,IDC_callCharge),WM_SETTEXT,0,(LPARAM)buf);
		wsprintf(buf,"%u",connTime-def_logInterval);/********************/
		SendMessage(GetDlgItem(hW,IDC_connTime),WM_SETTEXT,0,(LPARAM)buf);
		break;
	}
	return 0;
}

int getCurLink(HWND hW){
/*	returns the IDC value of the static text 'link' on which the
	mouse cursor is	currently on; if not on any, returns zero	*/
	POINT p;
	RECT r;

	GetCursorPos(&p);
	GetWindowRect(GetDlgItem(hW,IDC_http),&r);
	if (PtInRect(&r,p))
		return IDC_http;
	else{
		GetWindowRect(GetDlgItem(hW,IDC_mail),&r);
		if (PtInRect(&r,p))
			return IDC_mail;
		else{
			GetWindowRect(GetDlgItem(hW,IDC_new),&r);
			if (PtInRect(&r,p))
				return IDC_new;
		}
	}
	return 0;
}

HBRUSH hBr;
LRESULT CALLBACK about_wndproc(HWND hW,UINT msg,WPARAM wp,LPARAM lp){

#define backColor	0x00000000/*black*/
#define ilogColor	0x0000ff00/*green*/
#define byColor		0x0000ff00/*green*/
#define zyxwColor	0x0000ff00/*green*/
#define linkColor	0x00ff0000/*blue*/
#define infoColor	0x00c0c0c0/*grey*/

	switch(msg){
	case WM_SETCURSOR:
		switch(getCurLink(hW)){
		case IDC_http:
		case IDC_mail:
		case IDC_new:
			SetCursor(LoadCursor(0,IDC_CROSS));
			break;
		default:
			SetCursor(LoadCursor(0,IDC_ARROW));
			/*break;*/
		}
		return 1;
	case WM_CTLCOLORSTATIC:
		switch(GetDlgCtrlID((HWND)lp)){
		case IDC_http:
		case IDC_mail:
		case IDC_new:
			SetTextColor((HDC)wp,linkColor);
			break;
		case IDC_ilogViewer:
			/*
			SetTextColor((HDC)wp,ilogColor);
			break;
			*/
		case IDC_by:
			/*
			SetTextColor((HDC)wp,byColor);
			break;
			*/
		case IDC_zyxware:
			SetTextColor((HDC)wp,zyxwColor);
			break;
		/*case IDC_info:*/
		default:
			SetTextColor((HDC)wp,infoColor);
		}
		SetBkMode((HDC)wp,TRANSPARENT);
	case WM_CTLCOLORDLG:
		return (LRESULT)hBr;
		/*break;*/
	case WM_LBUTTONUP:
		switch(getCurLink(hW)){
		case IDC_http:
			ShellExecute(0,0,"http://zyxware.virtualave.net/",0,0,0);
			break;
		case IDC_mail:
			ShellExecute(0,0,"mailto:zyxware@yahoo.com?subject=[ilog_v0.8.1b]",0,0,0);
			break;
		case IDC_new:
			ShellExecute(0,0,"http://zyxware.virtualave.net/new/?57010530",0,0,0);
			break;
		default:
			goto closeDlg;
		}
		break;
	case WM_COMMAND:
closeDlg:
		DeleteObject(hBr);
		EndDialog(hW,0);
		break;
	case WM_INITDIALOG:
		GetWindowRect(hW,&r);
		wp=r.right-r.left;lp=r.bottom-r.top;
		GetWindowRect(GetParent(hW),&r);
		MoveWindow(hW,r.left+((r.right-r.left)-wp)/2,r.top+((r.bottom-r.top)-lp)/2,wp,lp,1);
		hBr=CreateSolidBrush(backColor);
		SetCursor(LoadCursor(0,IDC_CROSS));
		break;
	}
	return 0;
}

int showConfigDlg(HWND hWp)
{
	MSG		msg;
	/*RECT	r;*/
	GetWindowRect(hWp,&r);
	if (!(hWc=CreateDialogParam(0,(char *)IDD_config,hWp,config_wndproc,(r.bottom<<16)+(r.left&0xffff))))
		SendMessage(hWp,WM_CFGDONE,0,0);
  while (GetMessage(&msg,0,0,0)){
		if (!IsDialogMessage(hWc,&msg)){
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}
	DestroyWindow(hWc);
	hWc=0;
	SendMessage(hWp,WM_CFGDONE,msg.wParam/*whether config changed*/,0);
	return 0;
}

int index;
LRESULT CALLBACK dlgproc(HWND hW,UINT msg,WPARAM wp,LPARAM lp){
	switch(msg){
	case WM_NCHITTEST:
		SetWindowLong(hW,DWL_MSGRESULT,HTCAPTION);/*return HTCAPTION to the wndproc*/
		return 1;
	case WM_MOVING:
		if (hWc){
			GetWindowRect(hW,&r);
			PostMessage(hWc,WM_MAINDLGMOVE,((RECT*)lp)->left-r.left,((RECT*)lp)->top-r.top);
		}
		break;
	case WM_TIMER:
		EnableWindow(GetDlgItem(hW,IDC_zyxware+(index&0x0f)),0);
		/*ShowWindow(GetDlgItem(hW,IDC_zyxware+(index&0x0f)),1);*/
		if (index>=0x10)
			--index;
		else
			++index;
		if (!(index&0x0f) || index&0x08)
			index^=0x10;
		EnableWindow(GetDlgItem(hW,IDC_zyxware+(index&0x0f)),1);
		/*ShowWindow(GetDlgItem(hW,IDC_zyxware+(index&0x0f)),0);*/
		break;
	case WM_NOTIFY:
		switch(((LPNMHDR)lp)->code){
		case LVN_COLUMNCLICK: 
			ListView_SortItems(((NM_LISTVIEW *)lp)->hdr.hwndFrom,compare,(LPARAM)(((NM_LISTVIEW *)lp)->iSubItem));
			break;
		case LVN_ITEMCHANGED:
			if (((LPNMHDR)lp)->idFrom==IDC_monthLog){
				if (((NM_LISTVIEW *)lp)->uNewState & LVIS_SELECTED){
					ListView_GetItemText(((LPNMHDR)lp)->hwndFrom,curDay_j=((NM_LISTVIEW *)lp)->iItem,0,buf,bufSize);
					curDay=_atoi(buf);
					displayDayLog(hW);
				}
			}
			/*break;*/
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wp)){
		case IDC_config:
			if(SendMessage(GetDlgItem(hW,IDC_config),BM_GETCHECK,0,0))
				CreateThread(0,0,(LPTHREAD_START_ROUTINE)showConfigDlg,(void *)hW,0,&wp);
				/*hWc=CreateDialogParam(0,(char *)IDD_config,hW,config_wndproc,(r.bottom<<16)+(r.left&0xffff));*/
			else
				if (hWc){
					SendMessage(hWc,WM_CLOSE,0,0);
					hWc=0;
				}
			break;
		case IDC_about:
			DialogBoxParam(0,(char *)IDD_about,hW,about_wndproc,0);
			SendMessage(GetDlgItem(hW,IDC_about),BM_SETCHECK,BST_UNCHECKED,0);
			break;
		case IDC_year:
			if (HIWORD(wp)==EN_CHANGE){
        if (curYear || curMonth || curDay){
				  /*to prevent calling displayMonthLog, initially*/
					curYear=GetDlgItemInt(hW,IDC_year,0,0);
					displayMonthLog(hW,1);
				}
			}
			break;
		case IDC_month:
			switch(HIWORD(wp)){
			case CBN_SELCHANGE:
			/*if (HIWORD(wp)==CBN_SELCHANGE){*/
				curMonth=1+SendMessage(GetDlgItem(hW,IDC_month),CB_GETCURSEL,0,0);
				displayMonthLog(hW,1);
			/*}*/
				break;
			}
			break;
		case IDCANCEL:
			goto close;
			/*break;*/
		}
		break;
	case WM_INITDIALOG:
		SetWindowText(hW,"ilogViewer");
		SetWindowLong(GetDlgItem(hW,IDC_month),GWL_STYLE,GetWindowLong(GetDlgItem(hW,IDC_month),GWL_STYLE)|LBS_WANTKEYBOARDINPUT);
		SetTimer(hW,1,150,0);
		l=GetPrivateProfileString("config","callCharge",def_callCharge_str,buf,bufSize,path);
		_callCharge=0;
		while(l--){
			if (buf[l]=='.' && !_callCharge){
				buf[l]=0;
				_callCharge=_atoi(buf+l+1);
			}
			else
				if (buf[l]<'0' || buf[l]>'9'){
					callCharge=def_callCharge;
					WritePrivateProfileString("config","callCharge",def_callCharge_str,path);
					goto done;
				}
		}
		callCharge=_atoi(buf)*100+_callCharge;
done:
		pulseRate=GetPrivateProfileInt("config","pulseRate",def_pulseRate,path);
		if (!pulseRate){
			pulseRate=def_pulseRate;
			wsprintf(buf,"%u",pulseRate);
			WritePrivateProfileString("config","pulseRate",buf,path);
		}
		connTime=GetPrivateProfileInt("config","connTime",def_connTime,path);
		connTime+=def_logInterval;
		/*curDay=curMonth=curYear=0;*//*initialized; global variables*/
		displayMonthLog(hW,1);
		SetFocus(GetDlgItem(hW,IDC_monthLog));
		break;
	case WM_CFGDONE:
		SendMessage(GetDlgItem(hW,IDC_config),BM_SETCHECK,BST_UNCHECKED,0);
		if (wp)/*if there was change in config*/
			displayMonthLog(hW,0);
		break;
	case WM_CLOSE:
close:
		if (hWc)
			SendMessage(hWc,WM_CLOSE,0,0);
		EndDialog(hW,0);
		/*break;*/
	}
	return 0;
}