#include <Windows.h>
#include <shellscalingapi.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <direct.h> // _wmkdir for creating directory
#pragma comment(lib, "gdi32.lib") // 链接 GDI 库

using namespace std;

/**
 * 存放每块显示器的信息：包括监视器在虚拟坐标系中的矩形范围，以及设备名称/索引等
 */
struct MonitorInfo {
    HMONITOR hMonitor;
    wstring deviceName;  // 如果获取不到，可以用索引代替
    RECT rect;           // 显示器在虚拟坐标上的区域
    int index;
};

/**
 * 全局/静态量，用于EnumDisplayMonitors回调时收集信息
 */
static vector<MonitorInfo> gMonitors;

/**
 * EnumDisplayMonitors的回调函数
 */
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    static int monitorIndex = 0;

    MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(MONITORINFOEXW);
    if (GetMonitorInfoW(hMonitor, &monInfo)) {
        MonitorInfo m;
        m.hMonitor   = hMonitor;
        m.rect       = monInfo.rcMonitor; // 物理坐标
        m.index      = monitorIndex++;

        // 获取设备名称
        // 一般在 monInfo.szDevice 里，如果想区分可以用它，
        // 也可能是类似 "\\.\DISPLAY1"、"\\.\DISPLAY2" 这种。
        wstring devName(monInfo.szDevice);

        // 如果想用更好看的名字，可以做一层处理，否则直接拿这个
        if (!devName.empty()) {
            m.deviceName = devName;
        } else {
            // 如果获取不到，就用索引
            wstringstream ss;
            ss << L"MONITOR#" << m.index;
            m.deviceName = ss.str();
        }

        gMonitors.push_back(m);
    }
    return TRUE; // 继续枚举
}

/**
 * 初始化 DPI 感知
 */
void initializeDPI() {
    HMODULE hShcore = LoadLibrary(TEXT("Shcore.dll"));
    if (hShcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
        SetProcessDpiAwarenessFunc setDpiAwareness =
            (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (setDpiAwareness) {
            setDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }
        FreeLibrary(hShcore);
    } else {
        // 对于更旧的系统
        SetProcessDPIAware();
    }
}

/**
 * 截取指定矩形区域为位图
 */
HBITMAP captureMonitorRect(const RECT& rect)
{
    int width  = rect.right  - rect.left;
    int height = rect.bottom - rect.top;

    if (width <= 0 || height <= 0) {
        return nullptr;
    }

    // 获取整屏DC
    HDC hScreenDC = GetDC(NULL);
    // 创建内存DC
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    // 创建位图
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);

    if (hBitmap) {
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
        // 将屏幕内容复制到位图
        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, rect.left, rect.top, SRCCOPY);
        SelectObject(hMemoryDC, hOldBitmap);
    }

    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return hBitmap;
}

/**
 * 保存位图到文件 (BMP)
 */
int saveBitmap(HBITMAP hBitmap, const wstring& filename)
{
    if (!hBitmap) return 1;

    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPFILEHEADER bfHeader = {0};
    BITMAPINFOHEADER biHeader = {0};

    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biWidth = bmp.bmWidth;
    biHeader.biHeight = bmp.bmHeight;
    biHeader.biPlanes = 1;
    biHeader.biBitCount = bmp.bmBitsPixel;
    biHeader.biCompression = BI_RGB;

    // 计算图像数据大小(行对齐方式)
    DWORD dwBmpSize = ((bmp.bmWidth * biHeader.biBitCount + 31) / 32) * 4 * bmp.bmHeight;
    biHeader.biSizeImage = dwBmpSize;

    bfHeader.bfType = 0x4D42;  // 'BM'
    bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfHeader.bfSize = bfHeader.bfOffBits + biHeader.biSizeImage;

    // 分配内存
    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    if (!hDIB) return 2;
    char* lpbitmap = (char*)GlobalLock(hDIB);

    // 取出位图数据
    HDC hdc = GetDC(NULL);
    if (!GetDIBits(hdc, hBitmap, 0, bmp.bmHeight, lpbitmap, (BITMAPINFO*)&biHeader, DIB_RGB_COLORS)) {
        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        ReleaseDC(NULL, hdc);
        return 3;
    }
    ReleaseDC(NULL, hdc);

    // 写文件
    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        return 4;
    }

    DWORD written = 0;
    WriteFile(hFile, &bfHeader, sizeof(BITMAPFILEHEADER), &written, NULL);
    WriteFile(hFile, &biHeader, sizeof(BITMAPINFOHEADER), &written, NULL);
    WriteFile(hFile, lpbitmap, dwBmpSize, &written, NULL);

    CloseHandle(hFile);
    GlobalUnlock(hDIB);
    GlobalFree(hDIB);

    return 0;
}

/**
 * 获取当前日期(yyyy-mm-dd)，用来做文件夹名
 */
wstring getDateString()
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    wstringstream ss;
    ss << setw(4) << setfill(L'0') << st.wYear << L"-"
       << setw(2) << setfill(L'0') << st.wMonth << L"-"
       << setw(2) << setfill(L'0') << st.wDay;
    return ss.str();
}

/**
 * 获取当前时间(hh-mm-ss)，用来做文件名的一部分
 */
wstring getTimeString()
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    wstringstream ss;
    ss << setw(2) << setfill(L'0') << st.wHour << L"-"
       << setw(2) << setfill(L'0') << st.wMinute << L"-"
       << setw(2) << setfill(L'0') << st.wSecond;
    return ss.str();
}

int main()
{
    // 重定向标准输出到文件
    std::ofstream log("C:\\Users\\IWMAI\\OneDrive\\Programs\\C\\ScreenCapture\\output.log", std::ios::app);
    std::cout.rdbuf(log.rdbuf());  // 重定向标准输出到文件
    std::cerr.rdbuf(log.rdbuf());  // 重定向标准错误到文件

    initializeDPI();

    while (true) {
        SYSTEMTIME st;
        GetLocalTime(&st);

        // 判断是否在秒的00或30
        if (st.wSecond == 0 || st.wSecond == 30) {
            // （1）先枚举所有显示器信息
            gMonitors.clear();
            EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
            if (gMonitors.empty()) {
                wcout << L"未检测到显示器，可能是错误或特殊情况。" << endl;
            } else {
                // （2）为当天创建文件夹
                wstring dateFolder = L"D:\\screenCap\\" + getDateString();
                // 创建文件夹（如果已经存在则无事发生）
                _wmkdir(dateFolder.c_str());

                // （3）遍历每个显示器，截取并保存
                for (size_t i = 0; i < gMonitors.size(); i++) {
                    MonitorInfo& mInfo = gMonitors[i];
                    
                    // 如果设备名称里有类似 '\\.\DISPLAY1' 的反斜杠，可以替换掉
                    // 以免造成文件名问题。这里简单处理一下：
                    wstring devName = mInfo.deviceName;
                    for (auto &ch : devName) {
                        if (ch == L'\\' || ch == L'.' || ch == L':') {
                            ch = L'_';
                        }
                    }

                    // 如果拿不到设备名称，就用index
                    if (devName.empty()) {
                        wstringstream ss;
                        ss << L"MONITOR#" << i;
                        devName = ss.str();
                    }

                    // 拼接输出文件名： [日期文件夹]\[HH-mm-ss]_[devName].bmp
                    wstringstream filePath;
                    filePath << dateFolder << L"\\"
                             << getTimeString() << L"_" << devName << L".bmp";
                    
                    // 截图
                    HBITMAP hBitmap = captureMonitorRect(mInfo.rect);
                    if (hBitmap) {
                        saveBitmap(hBitmap, filePath.str());
                        DeleteObject(hBitmap);
                        wcout << L"已保存: " << filePath.str() << endl;
                    } else {
                        wcerr << L"截屏失败: " << devName << endl;
                    }
                }
            }

            // 为了避免 0.5 秒后再次进入重复截屏，这里可以多sleep几秒
            // 例如sleep 5秒，让时间超过这个点
            this_thread::sleep_for(chrono::seconds(5));
        } else {
            // 每次循环0.5秒
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    return 0;
}
