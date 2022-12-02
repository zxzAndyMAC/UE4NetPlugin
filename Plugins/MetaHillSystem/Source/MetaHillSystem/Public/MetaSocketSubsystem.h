// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <atomic>
#include <mutex>
#include <unordered_map>

#include "yasio/platform/yasio_unreal.hpp"
#include "yasio/yasio.hpp"
#include "yasio/obstream.hpp"
#include "yasio/yasio_fwd.hpp"

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "MetaSocketSubsystem.generated.h"

using namespace yasio;
using namespace yasio::inet;

DECLARE_LOG_CATEGORY_EXTERN(Meta_Network, Log, All);

#define METASOCKET_FUNC (FString(__FUNCTION__))              // Current Class Name + Function Name where this is called
#define METASOCKET_LINE (FString::FromInt(__LINE__))         // Current Line Number in the code where this is called
#define METASOCKET_FUNC_LINE (METASOCKET_FUNC + "(" + METASOCKET_LINE + ")") // Current Class and Line Number where this is called!

#pragma pack(push)
#pragma pack(1)
/**
 *通用命令头
 */
struct PACKET_CMD_HEAD
{
	uint8_t version = 0; //包头版本号
	uint8_t tag = 0; //预留
	uint8_t encrypted = 0; //是否已加密
	uint8_t compresed = 0; //是否已压缩
	uint16_t length = 0; //包体长度, 整个包的长度
	uint32_t cmd_id = 0; //命令ID
};
#pragma pack(pop)

struct UUID32
{
	transport_handle_t  trans; 
	FString				uuid;
};

/**
 *host
 */
USTRUCT(BlueprintType)
struct FMetaSocketHost
{
	GENERATED_USTRUCT_BODY()

	/**
	 * @brief ip地址
	 */
	UPROPERTY(BlueprintReadWrite)
	FString Ip;

	/**
	 * @brief 端口
	 */
	UPROPERTY(BlueprintReadWrite)
	int Port;
};

/**
 * TCP系统
 */
UCLASS()
class METAHILLSYSTEM_API UMetaSocketSubsystem : public UObject
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual ~UMetaSocketSubsystem() override;

	/**
	*  Get an instance of this library. This allows non-static functions to be called. 
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Meta|SocketClient")
		static UMetaSocketSubsystem* getMetaSocketClientTarget();
	static UMetaSocketSubsystem* socketClientBPLibrary;
	
public:	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMetaSocketTCPConnectedDelegate, FString, uuid32, int, channelId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMetaSocketReciveTCPMessageDelegate, FString, message, int, channelId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMetaSocketTCPDisConnectDelegate, int, channelId);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMetaSocketTCPHeartBeatDelegate);
	/**
	 * @brief 连接成功委托
	 */
	UPROPERTY(BlueprintAssignable, Category = "Meta|Socket|Events|Connected")
	FMetaSocketTCPConnectedDelegate onsocketClientTCPConnectedEventDelegate;

	/**
	 * @brief 收到消息委托
	 */
	UPROPERTY(BlueprintAssignable, Category = "Meta|Socket|Events|ReciveMsg")
	FMetaSocketReciveTCPMessageDelegate onsocketReciveTCPMessageDelegate;

	/**
	 * @brief 断开连接
	 */
	UPROPERTY(BlueprintAssignable, Category = "Meta|Socket|Events|DisConnect")
	FMetaSocketTCPDisConnectDelegate onsocketClientTCPDisConnectEventDelegate;

	/**
	 * @brief 心跳
	 */
	UPROPERTY(BlueprintAssignable, Category = "Meta|Socket|Events|HeartBeat")
	FMetaSocketTCPHeartBeatDelegate onsocketHeartBeatDelegate;
	
public:
	/**
	 * @brief 创建服务
	 * @param hosts FMetaSocketHost
	 * @param timeout 超时时间
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Create Service", ToolTip = "创建服务"), Category = "Meta|Socket")
	UMetaSocketSubsystem* CreateService(const TArray<FMetaSocketHost>& hosts, const int timeout);

	/**
	 * @brief 启动服务
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Start service", ToolTip = "启动网络服务"), Category = "Meta|Socket")
	UMetaSocketSubsystem* StartService();

	/**
	 * @brief 停止网络服务
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Stop service", ToolTip = "停止网络服务"), Category = "Meta|Socket")
	UMetaSocketSubsystem* StopService();

	/**
	 * @brief 打开信道
	 * @param 信道索引 
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Open channel", ToolTip = "打开信道"), Category = "Meta|Socket")
	UMetaSocketSubsystem* OpenChannel(int cindex = 0);

	/**
	 * @brief 关闭信道
	 * @param 信道索引 
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Close channel", ToolTip = "关闭信道"), Category = "Meta|Socket")
	UMetaSocketSubsystem* CloseChannel(int cindex = 0);

	/**
	 * @brief 分派网络线程产生的事件
	 * @param maxcount 本次最大分派事件数
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Dispatch", ToolTip = "分派网络线程产生的事件"), Category = "Meta|Socket")
	UMetaSocketSubsystem* Dispatch(const int maxcount = 128);

	/**
	 * @brief 获取信道传输字节数
	 * @param cindex 信道索引
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Get Bytes of Channel", ToolTip = "获取信道传输总字节数"), Category = "Meta|Socket")
	int64 GetBytesTransferredOfChannel(int cindex = 0);

	/**
	 * @brief 创建定时器
	 * @param delayed 延迟多少秒
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Create timer", ToolTip = "创建定时器"), Category = "Meta|Socket")
	UMetaSocketSubsystem* CreateTimer(int64 delayed);

	/**
	 * @brief 
	 * @param  cmdId 命令字
	 * @param  data  数据
	 * @param  cindex  信道索引
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Send Message", ToolTip = "发送消息"), Category = "Meta|Socket")
	UMetaSocketSubsystem* SendMessage(int32 cmdId, const FString& data, int cindex = 0);
private:
	/**
	 * @brief
	 * @param cindex 信道索引.
	 * @param ptr 待解包数据首字节地址.
	 * @param len 待解包数据长度。
	 * @return 返回消息包数据实际长度。	> 0: 解码包长度成功。
									== 0: 接收数据不足以解码消息包实际长度，yasio底层会继续接收数据，一旦收到新数据，会再次调用此函数。
									< 0: 解码包长度异常，会触发当前传输会话断开。
	 */
	int DecodeLen(int cindex, void* ptr, int len);

	/**
	 * @brief 清除
	 * @param  cindex 信道索引
	 */
	void ClearTrans(int cindex);
	
private:
	TSharedPtr<io_service> _service = nullptr;
	std::mutex			   _readMutex;

	std::unordered_map<int, UUID32>  _trans_map;
	std::atomic<bool>		_stoptimer {false};
	TSharedPtr<obstream> _obs = nullptr;
};
