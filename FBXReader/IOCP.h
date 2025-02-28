#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "Camera.h"
#include "BitmapQueue.h"

#pragma comment(lib, "ws2_32.lib")

//IOCP 가상클래스
class IOCP {
protected:
	WSADATA wsaData;
	HANDLE mhIOCP;

public:
	virtual bool Init(USHORT port) = 0;
	virtual void RunNetwork(void* cp) = 0;
protected:

	void SetThread(std::function<void(IOCP*, void*)> thread = &IOCP::RunNetwork) {
		//cpu코어수 알아내기
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		int numOfCore = sysInfo.dwNumberOfProcessors;

		//코어 수만큼 스레드 생성
		for (int i = 0; i < numOfCore * 2; ++i) {
			new std::thread(thread, this, mhIOCP);
		}
	}
};


enum IOCP_FLAG {
	IOCP_FLAG_READ,
	IOCP_FLAG_WRITE,
	IOCP_FLAG_MAX
};
enum COMMAND {
	COMMAND_HELLO = 0,
	COMMAND_REQ_FRAME,
	COMMAND_RES_FRAME,
	COMMAND_INPUT,
	COMMAND_MAX
};

enum INPUT_TYPE {
	INPUT_KEY_W,
	INPUT_KEY_S,
	INPUT_KEY_A,
	INPUT_KEY_D,
	INPUT_AXIS_CAMERA_MOVE,
	INPUT_AXIS_CAMERA_ROT,
	INPUT_MAX
};

struct INPUT_DATA {
	INPUT_TYPE mInputType;
	float deltaTime;
	float x;
	float y;
	float z;
	float w;
};

struct HEADER {
	DWORD64 mDataLen;
	DWORD64 mCommand;
	
};

//헤더 생성 보조 구조체
struct CHEADER : HEADER {
	CHEADER() {
		mDataLen = 0;
		mCommand = htonl(COMMAND::COMMAND_REQ_FRAME);
	}

	CHEADER(DWORD64 command) {
		mDataLen = 0;
		mCommand = htonl(command);
	}
	CHEADER(DWORD64 command, DWORD64 dataLen) {
		mDataLen = htonl(dataLen);
		mCommand = htonl(command);
	}
};

struct Packet {
	WSABUF mHeader;
	WSABUF mData;
	const DWORD64 headerSize = sizeof(HEADER);

	Packet(int dataSize = 0) {
		mHeader.buf = new char[headerSize];
		mHeader.len = 0;
		mData.buf = nullptr;
		if (dataSize != 0) {
			mData.buf = new char[dataSize];
			mData.len = dataSize;
		}
	}

	Packet(HEADER* header, void* data = nullptr) {
		mHeader.buf = (char*)header;
		mHeader.len = 0;
		mData.buf = nullptr;
		if (data != nullptr) {
			DWORD64 dataSize = ntohl(header->mDataLen);
			mData.buf = (char*)data;
			mData.len = dataSize;
		}
	}
	~Packet() {
		if (mHeader.buf != nullptr) {
			delete mHeader.buf;
		}
		if (mData.buf != nullptr) {
			delete mData.buf;
		}
	}
	void OutputPacketCommand() {
		HEADER* header = (HEADER*)mHeader.buf;

		char str[256];

		switch (ntohl(header->mCommand)) {
			case COMMAND::COMMAND_REQ_FRAME:
				sprintf(str, "COMMAND_REQ_FRAME");
				break;
			case COMMAND::COMMAND_INPUT:
				sprintf(str, "COMMAND_INPUT");
				break;
		}

		OutputDebugStringA(str);
	}
	void AllocDataBuffer(int size) {
		if (mData.buf == nullptr) {
			mData.buf = new char[size];
			mData.len = size;
		}
	}

};

//Overlapped 확장 구조체
struct OVERLAPPEDEX{
	OVERLAPPED mOverlapped;
	DWORD mFlag;
	DWORD mNumberOfByte;

	std::unique_ptr<Packet> mPacket = nullptr;
	const DWORD64 headerSize = sizeof(HEADER);
};

struct DeviceInfo {
	int mClientWidth;
	int mClientHeight;

	enum PixelOrder {
		RGBA = 0,
		BGRA
	};
	PixelOrder mClientPixelOreder;
};


//SocketInfo 구조체
struct SocketInfo {
	SOCKET socket;
	sockaddr_in clientAddr;

	Camera mCamera;

	DeviceInfo mDeviceInfo;

	QueueEX<std::unique_ptr<Packet>> rQueue;
	QueueEX<std::unique_ptr<Packet>> wQueue;
	QueueEX<std::unique_ptr<Packet>> inputRQueue;

	std::atomic<bool> isUsingRecv = false;
	std::atomic<bool> isUsingSend = false;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;	//CmdList Allocator
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;	//Command List

	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetBuffer;	//RenderTarget Buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;	//DepthStencil Buffer
};


//Server클래스
class IOCPServer : public IOCP {
public:
	IOCPServer(UINT maxClient) : maxClientCount(maxClient) { }
	virtual ~IOCPServer();

private:
	SOCKET listenSock;	//서버 듣기용 소켓
	sockaddr_in serverAddr;	//서버 주소
	std::vector<SocketInfo*> clients;	//접속된 클라이언트들

	const DWORD64 headerSize = sizeof(HEADER);

	UINT maxClientCount = 0;
public:
	bool Init(USHORT port) override;	//초기화
	void AcceptClient();	//Client 접속

	void RequestRecv(int sockIdx, bool overlapped = true);	//중첩소켓에 수신요청
	void RequestSend(int sockIdx, bool overlapped = true);	//중첩소켓에 송신요청

	INT GetClientNum() { return clients.size(); }
	SocketInfo* GetClient(int idx) { return clients[idx]; }
private:
	void RunNetwork(void* cp) override;	//스레드

	bool RecvHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize);	//Header 수신
	bool RecvData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx);	//Data 수신

	bool SendHeader(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx, DWORD nowSize);	//Header 송신
	bool SendData(SocketInfo* sInfo, OVERLAPPEDEX& overlappedEx);	//Data 송신
}; 

