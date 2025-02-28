#include "Receiver.h"
#include "GameTimer.h"
#include "lz4.h"
#include "QuickLZ.h"

#include <WindowsX.h>
#include <thread>
#include <string>
#include <DirectXMath.h>

HINSTANCE mhInst;	//인스턴스 핸들
HWND mhMainWnd;	//메인 윈도우 핸들

LPCWSTR clsName = TEXT("D3DReceiver");	//윈도우 쿨래스 네임

std::unique_ptr<Client> client = nullptr;	//클라이언트

UINT mClientWidth;
UINT mClientHeight;

std::thread* mNetworkReadThread = nullptr;
std::thread* mNetworkWriteThread = nullptr;

GameTimer mTimer;

LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
void Render();
void CalculateFrameStatus();
void Input(GameTimer& timer);
void OnMouseDown(WPARAM btnState, int x, int y);
void OnMouseUp(WPARAM btnState, int x, int y);
void OnMouseMove(WPARAM btnState, int x, int y);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nCmdShow) {


	//FIle을 읽어서 아이피와 포트번호 받아오기
	FILE* config = fopen("config.txt", "rt");
	if (config == nullptr) {
		MessageBoxA(NULL, "Can't Open File", "Error", MB_OK);
		return -1;
	}

	char serverIP[30];
	char serverPort[20];
	char windowWidth[20];
	char windowHeight[20];
	unsigned short serverPort_short;

	fscanf_s(config, "%*[Server_IP=]%s\n", serverIP, sizeof(serverIP));
	fscanf_s(config, "%*[Server_Port=]%s\n", serverPort, sizeof(serverPort));
	fscanf_s(config, "%*[WindowWidth=]%s\n", windowWidth, sizeof(windowWidth));
	fscanf_s(config, "%*[WindowHeight=]%s\n", windowHeight, sizeof(windowHeight));

	serverPort_short = atoi(serverPort);
	mClientWidth = atoi(windowWidth);
	mClientHeight = atoi(windowHeight);

	fclose(config);

	WNDCLASS wndCls;
	ZeroMemory(&wndCls, sizeof(wndCls));

	//메인 윈도우[메인 윈도우]
	wndCls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndCls.hCursor = (HCURSOR)LoadCursor(NULL, IDC_ARROW);
	wndCls.hIcon = (HICON)LoadIcon(NULL, IDI_APPLICATION);
	wndCls.hInstance = mhInst;
	wndCls.lpfnWndProc = (WNDPROC)WndProc;
	wndCls.lpszClassName = clsName;
	wndCls.lpszMenuName = NULL;
	wndCls.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClass(&wndCls);

	//실제 작업영역 받아오기
	RECT Rect;
	ZeroMemory(&Rect, sizeof(RECT));
	AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, false);
	int additionalWidth = Rect.left - Rect.right;
	int additionalHeight = Rect.bottom - Rect.top;

	//윈도우 생성
	mhMainWnd = CreateWindow(clsName,
		clsName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,	//xPos
		CW_USEDEFAULT,	//yPos
		mClientWidth + additionalWidth,	//Width
		mClientHeight + additionalHeight,	//Height
		NULL,
		NULL,
		mhInst,
		NULL);

	//윈도우 표시
	ShowWindow(mhMainWnd, SW_SHOW);

	//네트워크 초기화
	client = std::make_unique<Client>(serverIP, serverPort_short);
	if (!client->Init()) {
		::MessageBoxA(mhMainWnd, "네트워크 초기화 오류", "오류", MB_OK);
		return 1;
	}

	//서버 접속
	if (!client->Connection()) {
		::MessageBoxA(mhMainWnd, "네트워크 커넥션 오류", "오류", MB_OK);
		return 1;
	}

	//서버로 디바이스 정보 전달
	DeviceInfo dInfo;
	dInfo.mClientWidth = mClientWidth;
	dInfo.mClientHeight = mClientHeight;
	dInfo.mClientPixelOreder = DeviceInfo::PixelOrder::BGRA;
	Packet helloPacket = Packet(new CHEADER(COMMAND::COMMAND_HELLO, sizeof(DeviceInfo)), (void*)&dInfo) ;

	send(client->GetSocket(), (char*)& dInfo, sizeof(DeviceInfo), 0);


	//타이머 리셋
	mTimer.Reset();

	
	MSG msg = { 0 };

	//메세지 루프
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{

			if (mNetworkReadThread == nullptr) {
				mNetworkReadThread = new std::thread([&]() -> void {
					while (true) {

						Input(mTimer);

						client->PushPacketWQueue(std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_REQ_FRAME)));

						if (!client->SendMSG()) {
							//delete server;
							//server = nullptr;
							OutputDebugStringA("SendMSG Error\n");
							//break;
						}

					}

					});
			}

			if (mNetworkWriteThread == nullptr) {
				mNetworkWriteThread = new std::thread([&]() -> void {
					while (true) {
						if (!client->RecvMSG()) {
							//delete server;
							//server = nullptr;
							OutputDebugStringA("RecvMSG Error\n");
							//break;
						}

					}

					});
			}

			mTimer.Tick();

			if (client->SizeRQueue() > 0) {

				CalculateFrameStatus();

				Render();	//렌더링

				client->PopPacketRQueue();

			}
		}
	}


	return 0;
}

void Input(GameTimer& timer) {

	if (GetAsyncKeyState('W') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_W;
		data->deltaTime = timer.DeltaTime() ;

		OutputDebugStringA("W 입력\n");
		client->PushPacketWQueue(std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	if (GetAsyncKeyState('S') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_S;
		data->deltaTime = timer.DeltaTime();

		OutputDebugStringA("S 입력\n");
		client->PushPacketWQueue(std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	if (GetAsyncKeyState('A') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_A;
		data->deltaTime = timer.DeltaTime() ;

		OutputDebugStringA("A 입력\n");
		client->PushPacketWQueue(std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	if (GetAsyncKeyState('D') & 0x8000) {
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_KEY_D;
		data->deltaTime = timer.DeltaTime();

		OutputDebugStringA("D 입력\n");
		client->PushPacketWQueue(std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}
}

void Render() {

	if (client != nullptr) {
		HDC hdc, hMemDC;
		HBITMAP hBitmap, hOldBitmap;

		hdc = GetDC(mhMainWnd);
		hMemDC = CreateCompatibleDC(hdc);

		int srcDataSize = mClientHeight * mClientWidth * 4;
		/*
		//압축해제 - LZ4

		char* srcData = new char[srcDataSize];
		int size = LZ4_decompress_fast(client->GetData(), srcData, srcDataSize);
		*/

		//압축해제 - QuickLZ
		char* srcData = new char[srcDataSize];
		qlz_state_decompress stateDecomp;
		int size2 = qlz_decompress(client->GetData(), srcData, &stateDecomp);

		hBitmap = CreateBitmap(mClientWidth, mClientHeight, 1, 32, srcData);	//Bitmap생성

		hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
		BitBlt(hdc, 0, 0, mClientWidth, mClientHeight, hMemDC, 0, 0, SRCCOPY);
		SelectObject(hMemDC, hOldBitmap);
		DeleteDC(hMemDC);
		DeleteObject(hBitmap);

		delete[] srcData;
		ReleaseDC(mhMainWnd, hdc);
	}

}

void CalculateFrameStatus() {
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = std::wstring(clsName) +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

LRESULT WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {

	switch (iMsg)
	{
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, iMsg, wParam, lParam);

}
POINT lastMousePos;

void OnMouseDown(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		lastMousePos.x = x;
		lastMousePos.y = y;

		SetCapture(mhMainWnd);
	}
}
void OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos.x));
		float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos.y));

		//dx, dy각도 만큼 카메라 회전
		INPUT_DATA* data = new INPUT_DATA();
		int dataSize = sizeof(INPUT_DATA);
		memset(data, 0x00, dataSize);
		data->mInputType = INPUT_TYPE::INPUT_AXIS_CAMERA_ROT;
		data->z= dx;
		data->w = dy;

		OutputDebugStringA("Mouse 입력\n");
		client->PushPacketWQueue(std::make_unique<Packet>(new CHEADER(COMMAND::COMMAND_INPUT, dataSize), data));
	}

	lastMousePos.x = x;
	lastMousePos.y = y;
}