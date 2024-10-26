#include <iostream>
#include <Windows.h>

using namespace std;


HBITMAP captureScreen() {
    // Get the device context of the screen
    HDC hScreenDC = GetDC(NULL);
    // Create a compatible device context
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    // Get the width and height of the screen
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // Create a compatible bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);

    // Select the bitmap into the compatible device context
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    // Copy the screen to the compatible device context
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    // Restore the old bitmap
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return hBitmap;
}

int saveBitmap(HBITMAP hBitmap, wstring filename) {
    // Create a bitmap info header
    BITMAP bitmap;
    BITMAPINFOHEADER bitmapInfoHeader;
    BITMAPFILEHEADER bitmapFileHeader;
    DWORD dwBmpSize;
    HANDLE hFile;
    DWORD dwBytesWritten = 0;
    