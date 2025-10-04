#include <atlstr.h>
#include <string>

#define SCS_DOS_BINARY 1
#define PATH_SIZE 300
#define EXENAME "dosbox-x-launcher.exe"

char path[PATH_SIZE];
ULONG nChars;
CRegKey regKey;
void SetupKeyDefaultValue(std::string keyName, std::string valueData, std::string valueName = "")
{
    if (regKey.Open(HKEY_CURRENT_USER, keyName.c_str()) != ERROR_SUCCESS)
        regKey.Create(HKEY_CURRENT_USER, keyName.c_str());
    nChars = PATH_SIZE;
    if (regKey.Open(HKEY_CURRENT_USER, keyName.c_str()) != ERROR_SUCCESS || regKey.QueryStringValue(valueName.c_str(), path, &nChars) != ERROR_SUCCESS || path != valueData.c_str())
        regKey.SetStringValue(valueName.c_str(), valueData.c_str());
    regKey.Close();
}

char* GetDosBoxXPath()
{
    GetModuleFileName(NULL, path, sizeof(path));
    char *p=strrchr(path, '\\');
    if (p!=NULL) *(p+1)=0;
    else *path=0;
    strcat_s(path, "dosbox-x.exe");
    if (PathFileExists(path))
        return path;
    strcpy_s(path, "C:\\DOSBox-X\\dosbox-x.exe");
    if (PathFileExists(path))
        return path;
    return "";
}

bool IsMsDosExecutable(const char* exec)
{
    DWORD dwBinaryType;
    return GetBinaryType(exec, &dwBinaryType) && dwBinaryType == SCS_DOS_BINARY;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    bool paraerr = false;
    int numArgs = 0;
    LPWSTR* args;
    std::string launcherKey = "Applications\\" + std::string(EXENAME);
    std::string launcher    = "Software\\Classes\\" + launcherKey;
    if (strlen(lpCmdLine)) {
        int retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
        if (retval) {
            LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
            if (lpWideCharStr != NULL && MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval)) {
                args = CommandLineToArgvW(lpWideCharStr, &numArgs);
                free(lpWideCharStr);
                if (numArgs > 0 && (!_wcsicmp(args[0], L"/?") || !_wcsicmp(args[0], L"-?"))) {
                    MessageBox(0,"DOSBox-X Launcher.","Information",MB_OK);
                    return 0;
                }
                if (numArgs > 0 && (!_wcsicmp(args[0], L"/u") || !_wcsicmp(args[0], L"-u"))) {
                    if (regKey.Open(HKEY_CURRENT_USER, launcher.c_str()) == ERROR_SUCCESS && regKey.Open(HKEY_CURRENT_USER, "Software\\Classes\\Applications") == ERROR_SUCCESS && regKey != NULL)
                        regKey.RecurseDeleteKey(EXENAME);
                    SetupKeyDefaultValue("Software\\Classes\\.com",      "comfile");
                    SetupKeyDefaultValue("Software\\Classes\\.exe",      "exefile");
                    MessageBox(0,"DOSBox-X Launcher uninstalled.","Information",MB_OK);
                    return 0;
                }
            } else
                paraerr = true;
        } else
            paraerr = true;
    }
    std::string dosbox = "";
    if (regKey.Open(HKEY_CURRENT_USER, "SOFTWARE\\DOSBox-X") != ERROR_SUCCESS)
        regKey.Create(HKEY_CURRENT_USER, "SOFTWARE\\DOSBox-X");
    *path = 0;
    nChars = PATH_SIZE;
    if (regKey.Open(HKEY_CURRENT_USER, "SOFTWARE\\DOSBox-X") != ERROR_SUCCESS || regKey.QueryStringValue("Location", path, &nChars) != ERROR_SUCCESS || !PathFileExists(path)) {
        dosbox = std::string(GetDosBoxXPath());
        if (dosbox.size()) SetupKeyDefaultValue("Software\\DOSBox-X", dosbox, "Location");
    } else
        dosbox = std::string(path);
    SetupKeyDefaultValue(launcher+"\\DefaultIcon",              "\"%1\"");
    GetModuleFileName(NULL, path, sizeof(path));
    SetupKeyDefaultValue(launcher+"\\shell\\open\\command",     "\""+std::string(path)+"\" \"%1\" %2 %3 %4 %5 %6 %7 %8 %9");
    SetupKeyDefaultValue(launcher+"\\shell\\runas",             "", "HasLUAShield");
    SetupKeyDefaultValue(launcher+"\\shell\\runas\\command",    "\"%1\" %*");
    SetupKeyDefaultValue(launcher+"\\shell\\runas\\command",    "\"%1\" %*", "IsolatedCommand");
    SetupKeyDefaultValue("Software\\Classes\\.com",      launcherKey);
    SetupKeyDefaultValue("Software\\Classes\\.exe",      launcherKey);

    if (paraerr || numArgs <= 0) {
        MessageBox(0,"DOSBox-X Launcher installed.","Information",MB_OK);
        return 0;
    }

    std::string fileName = CW2A(args[0]);
    std::string fileType = "";
    if (fileName.size()>4 && !_stricmp(fileName.substr(fileName.size() - 4).c_str(), ".exe"))
        fileType = "exe";
    else if (fileName.size()>4 && !_stricmp(fileName.substr(fileName.size() - 4).c_str(), ".com"))
        fileType = "com";
    if (fileType == "")
        return 0;
    std::string fileArgs = "";
    for (int i=1; i<numArgs;i++) {
        fileArgs += CW2A(args[i]);
        if (i<numArgs-1) fileArgs += " ";
    }
    
    SetupKeyDefaultValue("Software\\Classes\\."+fileType, fileType+"file");

    if (IsMsDosExecutable(fileName.c_str())) {
        if (!dosbox.size()) {
            MessageBox(0,"Could not launch DOSBox-X.","Error",MB_OK);
            return 1;
        }
        strcpy_s(path, dosbox.c_str());
        char *p=strrchr(path, '\\');
        if (p!=NULL) *(p+1)=0;
        else *p=0;
        fileArgs = "-fastlaunch -defaultdir \""+(*p?std::string(p):".")+" \" \""+fileName+"\"";
        fileName = dosbox;
    }

    ShellExecute(NULL, _T("open"), fileName.c_str(), fileArgs.c_str(), NULL, SW_SHOW);

    SetupKeyDefaultValue("Software\\Classes\\."+fileType, launcherKey);
    return 0;
}
