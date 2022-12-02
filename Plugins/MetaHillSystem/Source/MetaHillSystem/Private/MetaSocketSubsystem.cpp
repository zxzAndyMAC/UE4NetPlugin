// Fill out your copyright notice in the Description page of Project Settings.


#include "MetaSocketSubsystem.h"

/**
 *最大包体长度
 */
constexpr unsigned int MAX_MSG_SIZE = 8192;


DEFINE_LOG_CATEGORY(Meta_Network);

using namespace yasio;
using namespace yasio::inet;

using self = UMetaSocketSubsystem*;

UMetaSocketSubsystem* UMetaSocketSubsystem::socketClientBPLibrary;

UMetaSocketSubsystem::UMetaSocketSubsystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {

	yasio_unreal_init();
	socketClientBPLibrary = this;

}

UMetaSocketSubsystem::~UMetaSocketSubsystem() {
	yasio_unreal_cleanup();
}

UMetaSocketSubsystem* UMetaSocketSubsystem::getMetaSocketClientTarget() {
	return socketClientBPLibrary;
}

self UMetaSocketSubsystem::CreateService(const TArray<FMetaSocketHost>& shosts, const int timeout)
{
	const int size = shosts.Num();
	std::vector<io_hostent> hosts;
	int index = 0;
	for (; index < size; index++)
	{
		hosts.push_back({TCHAR_TO_UTF8(*(shosts[index].Ip)), static_cast<u_short>(shosts[index].Port)});
	}

	UE_LOG(Meta_Network, Log, TEXT(">>>>> Create Service"));
	
	_service = MakeShared<io_service>(hosts);
	_obs = MakeShared<obstream>(MAX_MSG_SIZE);

	UE_LOG(Meta_Network, Log, TEXT(">>>>> Set Timeout"));
	
	_service->set_option(YOPT_S_CONNECT_TIMEOUT, timeout); //连接超时

	UE_LOG(Meta_Network, Log, TEXT(">>>>> Set Custom Decode"));
	index = 0;
	for (; index < size; index++)
	{
		_service->channel_at(index)->setDecode_len(
			[=](void* ptr, int len)
			{
				return this->DecodeLen(index, ptr, len);
			});
	}

	return this;
}

self UMetaSocketSubsystem::StartService()
{
	UE_LOG(Meta_Network, Log, TEXT(">>>>> StartService"));
	if (_service->is_running())
	{
		return this;
	}

	_service->start([&](event_ptr&& event)
	{
		std::lock_guard<std::mutex> lk(_readMutex);
		auto trans = event->transport();
		auto cindex = event->cindex();
		
		switch (event->kind())
		{
		case YEK_PACKET:
			{
				auto packet = std::move(event->packet());
				packet.push_back('\0');
				UE_LOG(Meta_Network, Log, TEXT(">>>>> Recive Packet"));
				if (_trans_map[cindex].uuid.IsEmpty())
				{
					FString body;
					body.AppendChars(packet.data(), 32);
					UE_LOG(Meta_Network, Log, TEXT("uuid: %s"), *body);
					_trans_map[cindex].uuid = body;
					
					onsocketClientTCPConnectedEventDelegate.Broadcast(body, cindex);
				}
				else
				{
					const ANSICHAR* ptr = packet.data();

					PACKET_CMD_HEAD head;
					constexpr int16 head_size = sizeof(PACKET_CMD_HEAD);
					
					std::memcpy(reinterpret_cast<char*>(&head), ptr, head_size);
					const int16 body_len = ntohs(head.length) - head_size;
					
					FString body;
					body.AppendChars(ptr + head_size, body_len);
					
					onsocketReciveTCPMessageDelegate.Broadcast(body, cindex);
					
				}

				break;
			}
		case YEK_ON_OPEN:
			if (event->status() == 0)
			{
				UE_LOG(Meta_Network, Log, TEXT(">>>>> Channel Opened"));
				std::unordered_map<int, UUID32>::const_iterator ret = _trans_map.find(cindex);
				if (ret == _trans_map.end())
				{
					UUID32 uuid32 = {trans, ""};
					_trans_map.insert(std::pair<int, UUID32>(cindex, uuid32));
				}
				//trans->local_endpoint().; //本机地址

				//get uuid
				obstream obs;
				obs.write(network_to_host(537006886, 4));
				_service->write(trans, std::move(obs.buffer()));
			}
			break;
		case YEK_ON_CLOSE:
			// printf("The connection is lost, %d bytes transferred\n", total_bytes_transferred);
			UE_LOG(LogTemp, Warning, TEXT("Connection is closed!!!"));
		//fix:
			onsocketClientTCPDisConnectEventDelegate.Broadcast(cindex);
			break;
		default:
			break;
		}
	});

	return this;
}

self UMetaSocketSubsystem::SendMessage(int32 cmdId, const FString& data, int cindex)
{
	UE_LOG(Meta_Network, Log, TEXT(">>>>> SendMessage：%s"), *data);
	
	const std::string msg(TCHAR_TO_UTF8(*data));

	PACKET_CMD_HEAD pHeader;
	pHeader.version = 1;
	pHeader.cmd_id = htonl(cmdId);
	pHeader.encrypted = 0;
	int16 len = sizeof(PACKET_CMD_HEAD) + msg.length();
	pHeader.length = htons(len);
	
	_obs->write_bytes(reinterpret_cast<char*>(&pHeader), sizeof(PACKET_CMD_HEAD));
	_obs->write_bytes(msg.c_str(), msg.length());

	_service->write(_trans_map[cindex].trans, std::move(_obs->buffer()));
	
	_obs->clear();

	return this;
}

self UMetaSocketSubsystem::StopService()
{
	UE_LOG(Meta_Network, Log, TEXT(">>>>> StopService"));
	
	if (_service->is_stopping())
	{
		return this;
	}

	if (_service->is_running())
	{
		_trans_map.clear();
		_stoptimer = true;
		_service->stop();
	}
	return this;
}

self UMetaSocketSubsystem::CreateTimer(int64 delayed)
{
	UE_LOG(Meta_Network, Log, TEXT(">>>>> CreateTimer: %d"), delayed);
	_stoptimer = false;
	_service->schedule(std::chrono::seconds(delayed), [&](io_service& service)-> bool
	{
		if (_stoptimer)
		{
			return true;
		}
		
		onsocketHeartBeatDelegate.Broadcast();
		
		return false;
	});

	return this;
}

self UMetaSocketSubsystem::OpenChannel(int cindex)
{
	UE_LOG(Meta_Network, Log, TEXT(">>>>> OpenChannel: %d"), cindex);
	_service->open(cindex, YCK_TCP_CLIENT);
	return this;
}

self UMetaSocketSubsystem::CloseChannel(int cindex)
{
	UE_LOG(Meta_Network, Log, TEXT(">>>>> CloseChannel: %d"), cindex);
	if (_service->is_open(cindex))
	{
		_service->close(cindex);
		ClearTrans(cindex);
	}
	return this;
}

int64 UMetaSocketSubsystem::GetBytesTransferredOfChannel(int cindex)
{
	return _service->channel_at(cindex)->bytes_transferred();
}

self UMetaSocketSubsystem::Dispatch(const int maxcount)
{
	if (_service && _service->is_running())
	{
		_service->dispatch(maxcount);
	}

	return this;
}

void UMetaSocketSubsystem::ClearTrans(int cindex)
{
	_trans_map.erase(cindex);
}

int UMetaSocketSubsystem::DecodeLen(int cindex, void* ptr, int len)
{
	//UE_LOG(Meta_Network, Warning, TEXT("===decode_len: %d"), len);
	UE_LOG(Meta_Network, Log, TEXT(">>>>> decode_len: %d"), len);
	std::lock_guard<std::mutex> lk(_readMutex);

	if (len == 0)
	{
		return 0;
	}

	if (_trans_map[cindex].uuid.IsEmpty())
	{
		if (len < 32)
		{
			return 0;
		}
		return len;
	}
	else
	{
		constexpr int16 head_size = sizeof(PACKET_CMD_HEAD);
		//UE_LOG(Meta_Network, Warning, TEXT("===head_length: %d"), length);
		if (len < head_size) //消息头长度不够
		{
			return 0;
		}

		PACKET_CMD_HEAD head;
		std::memcpy(reinterpret_cast<char*>(&head), ptr, head_size);
		const int16 body_len = ntohs(head.length) - head_size;
		
		//UE_LOG(Meta_Network, Warning, TEXT("===body_length: %d"), read_msg_.len_);
		const int16 length = head_size + body_len;
		if (len < length) //消息体长度不够
		{
			return 0;
		}else if(len > MAX_MSG_SIZE)
		{
			return -1;
		}
		return len;
	}
}
