# TYSocks

Socks5 Proxy based on TYProto

注意：由于比较赶时间，代码略乱，仅供测试。

目前只是 make it works ，可能会有 BUG， 如果遇到问题，请直接聊天 @ 我，或者新建一个 issue。

客户端和服务器是一对一的，一个服务端一个端口对应一个客户端

断开之后会退出，请自行重启客户端以重连，服务器需要最多 15 秒的时间来 cleanup 并重新等待

## 参数

请务必正确设置

* queue: 队列大小
* maxconn:  最大连接数（暂时没做限制，超了会炸）
* backend: 建议使用 epoll (Linux) 或者 kqueue (BSD)
* windowSize:  每次循环发送的数据包个数
* resendDelay: 丢包重发延迟，和 ACKDelay 加起来不要低于与服务器的延迟
* maxPPS: 每秒最大包数量，通常设置为带宽/包大小
* ACKDelay: 两个 ACK 之间最小时间间隔
* SACKDelay: 两个 SACK 之间最小时间间隔
* SACKThreshold: 收到的包 ID 跟最后一个确认 ID 差多少则发送 SACK
* resendTimeout: 重发超时时间，如果重发了 X 个包没有收到 ACK ，则终止连接

### 客户端配置
默认位置：/etc/tysocks/client

	[Remote]
	addr=127.0.0.1
	port=59999
	key=test
	pktSize=1400
	
	[Local]
	bind=127.0.0.1
	port=2333
	
	[TCP]
	queue=128
	maxconn=16384
	backend=kqueue
	
	[RemoteSockOpt]
	windowSize=500
	resendDelay=250
	maxPPS=50000
	ACKDelay=100
	SACKDelay=100
	SACKThreshold=10
	resendTimeout=2000
	
	[LocalSockOpt]
	windowSize=500
	resendDelay=250
	maxPPS=50000
	ACKDelay=100
	SACKDelay=100
	SACKThreshold=10
	resendTimeout=2000

服务器配置 /etc/tysocks/server:

	[Server]
	bind=127.0.0.1
	port=59999
	key=test
	pktSize=1400
	
	[TCP]
	maxconn=16384
	backend=kqueue
	conntimeout=3000
	
	[DNS]
	timeout=3000
	repeat=2