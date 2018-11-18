## USB WiFi适配记录——基于MT7601

### 安全性

无线网络的安全性由两个部分组成：认证和加密。

认证：使得只有允许的设备才能连接到无线网络。常用的认证算法有：开放认证、共享密钥认证、802.11x认证、PSK认证。其中802.11x和PSK认证的安全性比较高，分别用于企业和个人的环境。

加密：确保数据的保密性和完整性，即数据在传输过程中不会被篡改。常用的加密算法有：WEP加密、TKIP加密、CCMP加密算法。其中WEP加密和TKIP加密都是RC4的加密算法，安全性较低;CCMP加密采用AES对称加密算法，安全性较高。

常见的安全策略：

| 安全策略 | 认证方式 | 加密方式      | 备注                   |
| -------- | -------- | ------------- | ---------------------- |
| Open     | open     | open          | 开放WiFi，无任何加密   |
|          | open     | WEP           | 开放WiFi，仅数据加密   |
| WEP      | WEP      | WEP           | 共享密钥认证，容易破解 |
| WAP      | 802.11x  | TKIP/WEP      | 比较安全，用于企业     |
|          | PSK      | TKIP/WEP      | 比较安全，用于个人     |
| WAP2     | 802.11x  | CCMP/TKIP/WEP | 目前最安全，用于企业   |
|          | PSK      | CCMP/TKIP/WEP | 目前最安全，用于个人   |

### 选择无线网卡

* 根据WiFi无线网卡的VID和PID判断内核是否支持该无线网卡。

  ```bash
  wendy@wendy-PC:~$ lsusb 
  Bus 002 Device 010: ID 148f:7601 Ralink Technology, Corp. MT7601U Wireless Adapter
  ```

* [查询内核是否支持该设备](https://wireless.wiki.kernel.org/)

  ![查询内核是否支持MT7601u](https://s1.ax1x.com/2018/11/18/izjDVP.png)

  ![查看VID和PID是否被支持](https://s1.ax1x.com/2018/11/18/izvrLR.png)

  * 可见内核从4.2版本开始便支持这款USB WiFi设备

### 配置内核支持MT7601U无线网卡

* 在内核源码中搜索“0x7601”

  ```bash
  wendy@wendy-PC:$ grep "0x7601" drivers/net/wireless/ -nr
  drivers/net/wireless/mediatek/mt7601u/usb.c:28:	{ USB_DEVICE(0x148f, 0x7601) },
  ```

* 查看对应路径的Makefile

  ```bash
  wendy@wendy-PC:$ cat drivers/net/wireless/mediatek/mt7601u/Makefile 
  obj-$(CONFIG_MT7601U)	+= mt7601u.o
  
  mt7601u-objs	= \
  	usb.o init.o main.o mcu.o trace.o dma.o core.o eeprom.o phy.o \
  	mac.o util.o debugfs.o tx.o
  
  CFLAGS_trace.o := -I$(src)
  ```

  * 所以对应的宏是：`CONFIG_MT7601U`

* 进入内核目录，执行menuconfig，打开CONFIG_MT7601U

  ![内核支持MT7601U](https://s1.ax1x.com/2018/11/18/izxEpF.png)

### 使用buildroot移植应用

想要使用无线网卡，需要用到四个软件：

1. iw：可用于OPEN、WEP这两种“认证/加密”，以及扫描WIFI热点等
2. wpa_supplicant：可用于前文介绍的四种“认证/加密”
3. hostapd：能够使得无线网卡切换为AP模式
4. dhcp：STA模式使WiFi网卡动态获取IP，AP模式分配IP

![添加dhcp软件包](https://s1.ax1x.com/2018/11/18/FSSkM4.png)

![添加hostapd软件包](https://s1.ax1x.com/2018/11/18/FSSed1.png)

![添加iw软件包](https://s1.ax1x.com/2018/11/18/FSS5y4.png)

![添加wpa_supplicant软件包](https://s1.ax1x.com/2018/11/18/FSplt0.png)

### 添加firmware

除了需要内核支持MT7601U的驱动，另外还需要mt7601u.bin固件，否则加载驱动会报错。该固件可以前往[linux-firmware的仓库](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git)下载得到。将mt7601u.bin拷贝到linux系统中的/lib/firmware/目录下。

### 启动USB WiFi网卡

```bash
ifconfig wlan0 up
```

### iw的使用

* 列出WiFi网卡的性能

  ```bash
  iw list
  ```

* 扫描WiFI热点

  ```bash
  iw dev wlan0 scan
  iw dev wlan0 scan | grep SSID:
  ```

* 连接到开放AP

  ```bash
  iw wlan0 connect SSID
  ```

* 查看连接状态

  ```bash
  iw dev wlan0 link
  ```

* 断开WiFi连接

  ```bash
  iw wlan0 disconnect
  ```

此时只能ping路由器和局域网设备，如果要连外网，还需要如下操作：

1. 修改`/etc/resolv.conf`，添加DNS：`nameserver 192.168.1.1`
2. 设置网关，输入命令：`route add default gw 192.168.1.1`

### wpa_supplicant的使用

> wpa_supplicant主要是用来支持WEP，WPA/WPA2和WAPI无线协议和加密认证的。wpa_supplicant是一个连接、配置WiFi的工具，它主要包含`wpa_supplicant`（命令行模式）和`wpa_cli`（交互模式）两个程序。通常情况下，可以通过wap_cli来进行WiFi的配置与连接，如果有特殊的需要，可以编写应用程序直接调用wpa_supplicant的接口直接开发。
>
> WiFi名字和密码都会被保存到一个配置文件中，在Linux中，路径是`/etc/wpa_supplicant.conf`

#### 连接开放网络

* 向`/etc/wpa_supplicant.conf`加入

  ```bash
  network={
      ssid="open_ssid"
      key_mgmt=NONE
  }
  ```

* 初始化wpa_supplicant

  ```bash
  wpa_supplicant -B -d -i wlan0 -c /etc/wpa_supplicant.conf
  ```

* 查看连接状态

  ```bash
  wpa_cli -iwlan0 status
  ```

* 断开连接

  ```bash
  wpa_cli -iwlan0 disconnect
  killall wpa_supplicant
  ```

* 重新连接

  ```bash
  wpa_cli -iwlan0 reconnect
  ```

#### 连接加密网络

* 向`/etc/wpa_supplicant.conf`加入：

  ```bash
  network={
      ssid="ssid"
      psk="password"
  }
  ```

* 初始化wpa_supplicant

  ```bash
  wpa_supplicant -B -d -i wlan0 -c /etc/wpa_supplicant.conf
  ```

### dhclient的使用

连接好WiFi后，输入：

```bash
dhclient wlan0
```

### hostapd 使用

创建`/etc/hostapd.conf`配置文件

```bash
ctrl_interface=/ver/run/hostapd
ssid=suda-f1c100s
channel=1
interface=wlan0
driver=nl80211
macaddr_acl=0
auth_algs=1
wpa=3
wpa_passphrase=12345678
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
```

### dhcpd的使用

编辑/etc/dhcp/dhcpd.conf配置文件

```bash
subnet 192.168.2.0 netmask 255.255.255.0{
    range 192.168.2.10 192.168.2.100;
    option domain-name-servers 192.168.2.1;
    option routers 192.168.2.1;
}
```

### 启动WiFi热点

1. 启动无线网卡，并设置IP

   ```bash
   ifconfig wlan0 up
   ifconfig wlan0 192.168.2.1
   ```

2. 启动AP和DHCP

   ```bash
   hostapd -B /etc/hostapd.conf
   dhcpd -cf /etc/dhcp/dhcpd.conf wlan0
   ```

3. 对应的停止命令

   ```bash
   killall hostapd
   killall dhcpd
   ```

4. 查看热点状态

   ```bash
   hostapd_cli all_sta
   ```

5. 查看热点配置

   ```bash
   hostapd_cli get_config
   ```

6. 查看已经连接的设备

   ```bash
   iw dev wlan0 station dump
   ```

   









