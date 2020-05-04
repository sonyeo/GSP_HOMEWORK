#include "stdafx.h"
#include "Exception.h"
#include "Log.h"
#include "PacketInterface.h"
#include "ClientSession.h"
#include "Player.h"


#include "MyPacket.pb.h"


//@{ Handler Helper

typedef void(*HandlerFunc)(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream);

// ��Ŷ�� handle �� �� �ִ� handler ���̺��� �����.
static HandlerFunc HandlerTable[MAX_PKT_TYPE];

static void DefaultHandler(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream)
{
	//CRASH_ASSERT(false);
	//session->DisconnectRequest(DR_NONE);
	printf_s("Default Handler...PKT ID: %d\n", pktBase.mType);
}

// �⺻ �ڵ鷯���� ���̺� ���
struct InitializeHandlers
{
	InitializeHandlers()
	{
		for (int i = 0; i < MAX_PKT_TYPE; ++i)
			HandlerTable[i] = DefaultHandler;
	}
} _init_handlers_;

// ���ο� �ڵ鷯�� ���
struct RegisterHandler
{
	RegisterHandler(int pktType, HandlerFunc handler)
	{
		HandlerTable[pktType] = handler;
	}
};

// �� �Ʒ��� ���� �ڵ鷯 ���Ǹ� ���� �ɵ�!
#define REGISTER_HANDLER(PKT_TYPE)	\
	static void Handler_##PKT_TYPE(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream); \ // ���漱��
	static RegisterHandler _register_##PKT_TYPE(PKT_TYPE, Handler_##PKT_TYPE); \ // �ڵ鷯 ���
	static void Handler_##PKT_TYPE(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream)
		

//@}

void ClientSession::OnReceive(size_t len)
{
	TRACE_THIS;

	/// ��Ŷ �Ľ��ϰ� ó��
	protobuf::io::ArrayInputStream arrayInputStream(mRecvBuffer.GetBufferStart(), mRecvBuffer.GetContiguiousBytes());
	protobuf::io::CodedInputStream codedInputStream(&arrayInputStream);

	PacketHeader packetheader;

	while (codedInputStream.ReadRaw(&packetheader, HEADER_SIZE))
	{
		const void* payloadPos = nullptr;
		int payloadSize = 0;

		codedInputStream.GetDirectBufferPointer(&payloadPos, &payloadSize);

		if ( payloadSize < packetheader.mSize ) ///< ��Ŷ ��ü ������ üũ
			break;

		if (packetheader.mType >= MAX_PKT_TYPE || packetheader.mType <= 0)
		{
			DisconnectRequest(DR_ACTIVE);
			break;;
		}

		/// payload �б�
		protobuf::io::ArrayInputStream payloadArrayStream(payloadPos, packetheader.mSize);
		protobuf::io::CodedInputStream payloadInputStream(&payloadArrayStream);

		// session�� this�� �־���
		/// packet dispatch...
		HandlerTable[packetheader.mType](this, packetheader, payloadInputStream);
	
		/// ���� ��ŭ ���� �� ���ۿ��� ����
		codedInputStream.Skip(packetheader.mSize); ///< readraw���� ��� ũ�⸸ŭ �̸� �����߱⶧��
		mRecvBuffer.Remove(HEADER_SIZE + packetheader.mSize);

	}
}

/////////////////////////////////////////////////////////////

using namespace MyPacket;

// �̷��� �ڵ鷯 ����ؼ� ���
REGISTER_HANDLER(PKT_CS_LOGIN)
{
	LoginRequest loginRequest;
	if (false == loginRequest.ParseFromCodedStream(&payloadStream))
	{
		session->DisconnectRequest(DR_ACTIVE);
		return;
	}

	session->mPlayer->DoSyncAfter(5000, &Player::RequestLoad, loginRequest.playerid());
}