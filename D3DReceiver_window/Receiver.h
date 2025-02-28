#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <atomic>
#include "BitmapQueue.h"

#pragma comment(lib, "ws2_32.lib")


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
	~CHEADER() = default;
};

struct Packet {
	WSABUF mHeader;
	WSABUF mData;
	const DWORD64 headerSize = sizeof(HEADER);

	Packet() {
		mHeader.buf = new char[headerSize];
		mHeader.len = headerSize;
		mData.buf = nullptr;
	}

	Packet(HEADER* header, void* data = nullptr) {
		mHeader.buf = (char*)header;
		mHeader.len = headerSize;
		mData.buf = nullptr;
		if (data != nullptr) {
			DWORD64 dataSize = ntohl(header->mDataLen);
			mData.buf = (char*)data;
			mData.len = dataSize;
		}
	}
	~Packet() {
		if (mHeader.buf != nullptr)
			delete mHeader.buf;
		if (mData.buf != nullptr)
			delete[] mData.buf;
	}

	void AllocDataBuffer(int size) {
		if (mData.buf == nullptr) {
			mData.buf = new char[size];
			mData.len = size;
		}
	}

};

struct DeviceInfo {
	enum PixelOrder {
		RGBA = 0,
		BGRA
	};

	int mClientWidth;
	int mClientHeight;
	PixelOrder mClientPixelOreder;
};

class Client {
public:
	Client(char* ip, short port);
	virtual ~Client();

private:
	WSADATA wsaData;
	SOCKET serverSock;
	sockaddr_in serverAddr;

	char serverIP[30];
	short serverPort;

	QueueEX<std::unique_ptr<Packet>> rQueue;
	QueueEX<std::unique_ptr<Packet>> wQueue;
	QueueEX<std::unique_ptr<Packet>> inputWQueue;

	std::atomic<bool> isUsingWQueue = false;
	std::atomic<bool> isUsingInputWQueue = false;
	std::atomic<bool> isUsingRQueue = false;
	std::atomic<int> reqFrameCount = 0;

	DWORD64 headerSize = sizeof(HEADER);

public:
	bool Init();
	bool Connection();

	bool RecvMSG();
	bool SendMSG();

	void PushPacketWQueue(std::unique_ptr<Packet> packet);
	void PopPacketRQueue();

	int SizeRQueue() { return rQueue.Size(); }
	int SizeWQueue() { return wQueue.Size(); }

	char* GetData();
	int GetDataSize();
	SOCKET GetSocket();

public:
	bool RecvHeader(Packet* packet);
	bool RecvData(Packet* packet);

	bool SendHeader(Packet* packet);
	bool SendData(Packet* packet);
};