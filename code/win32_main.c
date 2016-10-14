#pragma warning(push, 0)
#include "windows.h"
#include "assert.h"
#include "dsound.h"
#include "stdint.h"
#define _USE_MATH_DEFINES
#include <math.h>
#pragma warning(pop)

#define SUPERPOSITION_IMPLEMENTATION
#include "superposition.h"

#pragma warning(push, 3)

typedef int                b32;
typedef unsigned char      u8;
typedef char               s8;
typedef unsigned short     u16;
typedef short              s16;
typedef unsigned int       u32;
typedef int                s32;
typedef unsigned long long u64;
typedef long long          s64;
typedef float              r32;
typedef double             r64;

typedef struct 
{
	HWND     Handle;
	int      Width;
	int      Height;
	HDC      DeviceContext;
} win32_window;

typedef struct 
{
	win32_window    Window;
} win32_state;

LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT Message, WPARAM wParam, LPARAM lParam)
{
	// Save us from powerdowns and screensavers
	if(Message==WM_SYSCOMMAND && (wParam==SC_SCREENSAVE || wParam==SC_MONITORPOWER))
		return(0);

	if(Message==WM_PAINT)
	{
		LPPAINTSTRUCT PaintInfo = 0;
		BeginPaint(WindowHandle, PaintInfo);

		HDC DeviceContext = GetDC(WindowHandle);

		RECT ClientRect;
		GetClientRect(WindowHandle, &ClientRect);
		int WindowWidth = ClientRect.right - ClientRect.left;
		int WindowHeight = ClientRect.bottom - ClientRect.top;

		// TODO(rb): Do drawing here?

		ReleaseDC(WindowHandle, DeviceContext);

		EndPaint(WindowHandle, PaintInfo);
		return(0);
	}

	return(DefWindowProc(WindowHandle, Message, wParam, lParam));
}

#define SuperpositionMemSize 100*1024
char SuperpositionMem[SuperpositionMemSize];

void* GiveMeMemory(int NumBytes)
{
	return VirtualAlloc(0, NumBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void* LoadFileData(const char* Path)
{
	WIN32_FILE_ATTRIBUTE_DATA FileData;
	BOOL Result = GetFileAttributesEx(Path, GetFileExInfoStandard, &FileData);

	b32 FileExists = 1;
	if (!Result) {
		DWORD Error = GetLastError();

		if (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND)
			FileExists = 0;
	}

	if (!FileExists) {
		OutputDebugStringA("Could not find file ");
		OutputDebugStringA(Path);
		OutputDebugStringA("\n");
		return (void*)0;
	}

	HANDLE File = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	LARGE_INTEGER Size;
	GetFileSizeEx(File, &Size);

	void* Data = GiveMeMemory(Size.LowPart);

	DWORD BytesRead = 0;
	ReadFile(File, Data, Size.LowPart, &BytesRead, NULL);
	CloseHandle(File);

	return Data;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	win32_state Win32;

	Win32.Window.Width = 500;
	Win32.Window.Height = 400;

	WNDCLASS WindowClass = {
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		&WindowProc,
		0,
		0,
		Instance,
		0,
		0,
		0,
		0,
		"superposition_window_class_name",
	};

	RegisterClassA(&WindowClass);

	RECT WindowRect = {0, 0, Win32.Window.Width, Win32.Window.Height};
	DWORD WindowStyle = WS_CAPTION | WS_VISIBLE | WS_SYSMENU; 

	AdjustWindowRect(&WindowRect, WindowStyle, 0);

	int WindowWidth = WindowRect.right - WindowRect.left;
	int WindowHeight = WindowRect.bottom - WindowRect.top;

	Win32.Window.Handle = CreateWindowA(
		"superposition_window_class_name",
		"superposition_test",
		WindowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		WindowWidth,
		WindowHeight,
		0,
		0,
		Instance,
		0
	);

	Win32.Window.DeviceContext = GetDC(Win32.Window.Handle);

	HBRUSH BlackBrush = CreateSolidBrush(RGB(0, 0, 0));
	FillRect(Win32.Window.DeviceContext, &WindowRect, BlackBrush);
	DeleteObject(BlackBrush);

	b32 IsRunning = 1;

	sp_system* Superposition = sp_open(Win32.Window.Handle, (void*)SuperpositionMem, SuperpositionMemSize);
	sp_update(Superposition, 1.f/60.f * 2.f);

	void* SomeWav = LoadFileData("C:\\Users\\rb\\Downloads\\[99Sounds] Cinematic Sound Effects\\[99Sounds] Cinematic Sound Effects\\Loops\\Generdyn - INSTLoops - 01.wav");

	int SomeWavSize = sp_unpacked_size(Superposition, SomeWav);
	void* SomeWavUnpacked = GiveMeMemory(SomeWavSize);
	sp_sample* SomeSample = sp_unpack(Superposition, SomeWav, SomeWavUnpacked);
	sp_play(Superposition, SomeSample);

	while (IsRunning)
	{
		MSG Message;

		int PosX, PosY;

		while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
		{
			switch (Message.message)
			{
				case WM_QUIT:
				{
					IsRunning = 0;
				} break;

				// TODO(rb): These are being handled both places
				case WM_SYSKEYDOWN:
				case WM_SYSKEYUP:
				case WM_KEYDOWN:
				case WM_KEYUP:
				{
					WPARAM VKCode = Message.wParam;
					b32 WasDown = ((Message.lParam & (1 << 30)) != 0);
					b32 IsDown = ((Message.lParam & (1 << 31)) == 0);

					if (WasDown != IsDown)
					{
						switch (VKCode)
						{
							case VK_LEFT:
							{
							} break;
							case VK_RIGHT:
							{
							} break;
							case VK_UP:
							{
							} break;
							case VK_DOWN:
							{
							} break;
							case VK_CONTROL:
							{
							} break;
						}
					}
				} break;

				case WM_MOUSEMOVE:
				{
					PosX = LOWORD(Message.lParam);
					PosY = HIWORD(Message.lParam);
				} break;

				case WM_LBUTTONDOWN:
				{
					SetCapture(Win32.Window.Handle);

					PosX = LOWORD(Message.lParam);
					PosY = HIWORD(Message.lParam);
				} break;

				case WM_LBUTTONUP:
				{
					ReleaseCapture();

					PosX = LOWORD(Message.lParam);
					PosY = HIWORD(Message.lParam);
				} break;

				case WM_CHAR:
				{
					if(Message.wParam==VK_ESCAPE)
					{
						PostQuitMessage(0);
						return(0);
					}
				} break;

			}

			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}


		sp_update(Superposition, 1.f/60.f * 2.f);

		Sleep(0);
	}

	sp_close(Superposition);

	return 0;
}
#pragma warning(pop)

