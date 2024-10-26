#include <iostream>
#include <Windows.h>
#include <shellscalingapi.h>
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

void initializeDPI() {
    // 针对 Windows 8.1 及以上使用 SetProcessDpiAwareness
    HMODULE hShcore = LoadLibrary(TEXT("Shcore.dll"));
    if (hShcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
        SetProcessDpiAwarenessFunc setDpiAwareness = (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (setDpiAwareness) {
            setDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }
        FreeLibrary(hShcore);
    } else {
        // 对于较旧的系统使用 SetProcessDPIAware
        SetProcessDPIAware();
    }
}

HBITMAP captureScreen() {
    // 获取屏幕的设备上下文
    HDC hScreenDC = GetDC(NULL);
    // 创建兼容的内存设备上下文
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    // 获取真实的屏幕分辨率
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // 创建一个兼容的位图
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);

    // 将位图选入内存设备上下文中
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    // 将屏幕内容复制到位图中
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    // 恢复原始位图并释放资源
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return hBitmap;
}

int saveBitmap(HBITMAP hBitmap, wstring filename) {
    if (!hBitmap) return 1;

    // Create a bitmap info header
    BITMAP bitmap;
    BITMAPINFOHEADER bitmapInfoHeader;
    BITMAPFILEHEADER bitmapFileHeader;
    DWORD dwBmpSize;
    HANDLE hFile;
    DWORD dwBytesWritten = 0;

    // Get the bitmap info
    GetObject(hBitmap, sizeof(BITMAP), &bitmap);

    // Initialize the bitmap info header
    bitmapInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfoHeader.biWidth = bitmap.bmWidth;
    bitmapInfoHeader.biHeight = bitmap.bmHeight;
    bitmapInfoHeader.biPlanes = 1;
    bitmapInfoHeader.biBitCount = bitmap.bmBitsPixel;  // Use the actual bit count
    bitmapInfoHeader.biCompression = BI_RGB;
    bitmapInfoHeader.biSizeImage = 0;
    bitmapInfoHeader.biXPelsPerMeter = 0;
    bitmapInfoHeader.biYPelsPerMeter = 0;
    bitmapInfoHeader.biClrUsed = 0;
    bitmapInfoHeader.biClrImportant = 0;

    // Calculate the size of the bitmap
    dwBmpSize = ((bitmap.bmWidth * bitmapInfoHeader.biBitCount + 31) / 32) * 4 * bitmap.bmHeight;

    // Allocate memory for the bitmap
    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    if (!hDIB) return 2;
    char* lpbitmap = (char*)GlobalLock(hDIB);

    // Copy the bitmap data into the memory
    HDC hdc = GetDC(NULL);
    if (!GetDIBits(hdc, hBitmap, 0, (UINT)bitmap.bmHeight, lpbitmap, (BITMAPINFO*)&bitmapInfoHeader, DIB_RGB_COLORS)) {
        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        ReleaseDC(NULL, hdc);
        return 3;
    }
    ReleaseDC(NULL, hdc);

    // Create the bitmap file
    hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        return 4;
    }

    // Write the bitmap file header
    bitmapFileHeader.bfType = 0x4D42; // BM
    bitmapFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bitmapFileHeader.bfReserved1 = 0;
    bitmapFileHeader.bfReserved2 = 0;
    bitmapFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    WriteFile(hFile, &bitmapFileHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, &bitmapInfoHeader, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

    // Release the resources
    CloseHandle(hFile);
    GlobalUnlock(hDIB);
    GlobalFree(hDIB);

    return 0;
}

wstring getFileName(const wstring& path) {
    SYSTEMTIME time;
    GetLocalTime(&time); // 获取本地时间

    wstringstream ss;
    ss << path << L"\\"; // 指定路径
    ss << setw(2) << setfill(L'0') << time.wYear % 100 << L"-"  // 年份的最后两位
       << setw(2) << setfill(L'0') << time.wMonth << L"-"       // 月
       << setw(2) << setfill(L'0') << time.wDay << L"-"         // 日
       << setw(2) << setfill(L'0') << time.wHour << L"-"        // 小时
       << setw(2) << setfill(L'0') << time.wMinute << L"-"      // 分
       << setw(2) << setfill(L'0') << time.wSecond << L".bmp";  // 秒 + 扩展名

    return ss.str();
}

int main() {
    initializeDPI(); // 初始化 DPI 感知

    while (true) {
        // 生成带当前时间的文件名
        wstring filename = getFileName(L"D:\\screenCap");

        // 截屏并保存
        HBITMAP hBitmap = captureScreen();
        if (hBitmap) {
            saveBitmap(hBitmap, filename);
            DeleteObject(hBitmap);
            wcout << L"Screenshot saved as " << filename << endl;
        } else {
            wcerr << L"Failed to capture screen on attempt " << filename << endl;
        }

        // 每次循环暂停 30 秒
        Sleep(30000);
    }

    return 0;
}
