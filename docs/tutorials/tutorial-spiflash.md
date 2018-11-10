# f1c100s spi-flash版教程
### 定制交叉编译工具链——使用crosstool-ng脚本

1. 安装必要的依赖软件

   `apt-get install gcc gperf bison flex texinfo help2man make libncurses5-dev python-dev`

2. 进入crosstool-ng根目录下，运行`./bootstrap`

3. 执行安装之前的配置

   ```bash
   ./configure --prefix=/some/place
   make && make install
   ```

4. 添加环境变量

   ```bash
   export PATH=$PATH:/some/place/ct-ng/bin
   source /some/place/ct-ng/share/bash-completion/completions/ct-ng
   ```

5. 查看示例，选择arm-unknown-linux-musleabi，然后微调配置

   ```bash
   ct-ng list-samples
   ct-ng arm-unknown-linux-musleabi
   ct-ng menuconfig
   ```

   ![Path and misc options](https://s1.ax1x.com/2018/09/15/iVV0ln.png)

   ![Target options](https://s1.ax1x.com/2018/09/15/iVVNFg.png)

   ![Toolchain options](https://s1.ax1x.com/2018/09/15/iVVByq.png)

   ![Operating System子配置菜单](https://s1.ax1x.com/2018/09/15/iVELMq.png)

6. 编译`ct-ng build`



## 实验板——荔枝派nano

![引脚映射图](http://odfef978i.bkt.clouddn.com/Pin%20Map.png)



## SPI-Flash分区规划

| 分区序号 | 分区大小        | 分区作用   | 地址空间及分区名               |
| -------- | --------------- | ---------- | ------------------------------ |
| mtd0     | 1MB (0x100000)  | spl+uboot  | 0x0000000-0x0100000 : “uboot”  |
| mtd1     | 64KB (0x10000)  | dtb文件    | 0x0100000-0x0110000 : “dtb”    |
| mtd2     | 4MB (0x400000)  | linux内核  | 0x0110000-0x0510000 : “kernel” |
| mtd3     | 剩余 (0xAF0000) | 根文件系统 | 0x0510000-0x1000000 : “rootfs” |



## u-boot

### 目录结构

```bash
.
├── api             //封装一些平台无关的操作，如字符串打印，显示，网络，内存
├── arch            //以平台架构区分
│   ├──arm
│   │   └──cpu
│   │   │   └──arm926ejs
│   │   │   │   └──sunxi   //cpu相关的一些操作，如定时器读取
│   │   │   │   │   └──u-boot-spl.lds  //spl的放置方法
│   │   └──dts
│   │   │   └──suniv-f1c100s-licheepi-nano.dts   // f1c100s芯片的一些配置
│   │   │   └──suniv-f1c100s-licheepi-nano.dtb
│   │   │   └──suniv-f1c100s.dtsi
│   │   │   └──suniv.dtsi
│   │   └──lib      //一些库文件
│   │   └──mach-sunxi
│   │   │   └──board.c          //board_init_f
│   │   │   └──dram_sun4i.c     //ddr的操作，复位，时钟，延时，odt，etc.
│   │   │   └──dram_helpers.c   //ddr的设置及读写测试
├── board
│   ├──sunxi
│   │   └──board.c              //sunxi_board_init 入口
│   │   └──dram_suniv.c        //DRAM的一些默认参数
├── cmd             //Uboot命令行的一些命令
├── common          //含spl
├── configs         //menuconfig里的默认配置,比如各类驱动适配
│   ├── licheepi_nano_defconfig
│   ├── licheepi_nano_spiflash_defconfig
├── disk            //硬盘分区的驱动
├── doc
├── drivers         //外设驱动
├── dts
├── examples
├── fs              //多种文件系统
├── include
│   ├──configs
│   │   └──sunxi_common.h   //预配置的参数，如串口号等
│   │   └──suniv.h
├── lib             //加密压缩等算法
├── net             //nfs,tftp等网络协议
├── post
├── scripts
```

### 安装依赖软件包

`sudo apt-get install swig`

### 配置&编译

1. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- licheepi_nano_spiflash_defconfig` 启用默认配置

2. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- menuconfig` 微调配置

   ![配置液晶显示屏参数](https://s1.ax1x.com/2018/09/18/iZvB5t.png)

   ![指定设备树文件](https://s1.ax1x.com/2018/10/03/i3lqwq.png)

   ![配置终端提示符](https://s1.ax1x.com/2018/09/18/iZvy28.png)

   ![环境变量配置](https://s1.ax1x.com/2018/10/03/i3fHG6.png)

3. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- `

### 添加u-boot开机画面

1. `sudo apt-get install netpbm`

2. `jpegtopnm logo-uboot.jpg | ppmquant 31 | ppmtobmp -bpp 8 > sunxi.bmp`，这里的sunxi代表u-boot中`board`环境变量的值

3. 将生成的bmp文件放入tools/logos文件夹下

4. 在配置文件`vim include/configs/sunxi-common.h`中添加如下信息

   ```c
   #define CONFIG_VIDEO_LOGO
   #define CONFIG_VIDEO_BMP_LOGO
   #define CONFIG_HIDE_LOGO_VERSION
   ```

### 设置bootcmd和bootargs

![设置bootargs](https://s1.ax1x.com/2018/10/03/i3f2xU.png)

* `console=tty0 console=ttyS0,115200 panic=5 rootwait root=/dev/mtdblock3 rw rootfstype=jffs2`

![设置bootcmd](https://s1.ax1x.com/2018/10/03/i3ffr4.png)

```bash
"sf probe 0:0 60000000;"
"sf read 0x80C00000 0x100000 0x10000;"
"sf read 0x80008000 0x110000 0x400000;"
"bootz 0x80008000 - 0x80C00000"
```

* 挂载spi-flash
* 读取 spi-flash 1M（0x100000）位置 64KB(0x4000)大小的 dtb 到地址 0x80C00000
* 读取 spi-flash 1M+64K（0x110000）位置 4MB(0x400000)大小的 zImage 到地址 0x80008000
* 从 0x80008000 启动内核，从 0x80C00000 读取设备树配置

### 烧写uboot到spi-flash中（使用sunxi-tools工具）

```bash
sudo sunxi-fel -p spiflash-write 0 u-boot-sunxi-with-spl.bin 
100% [================================================]  1008 kB,   95.3 kB/s 
```

### 启动日志

```bash
U-Boot SPL 2018.01suda-05679-g013ca457fd-dirty (Oct 03 2018 - 23:55:33)
DRAM: 32 MiB
Trying to boot from MMC1
Card did not respond to voltage select!
mmc_init: -95, time 22
spl: mmc init failed with error: -95
Trying to boot from sunxi SPI


U-Boot 2018.01suda-05679-g013ca457fd-dirty (Oct 03 2018 - 23:55:33 +0800)

CPU:   Allwinner F Series (SUNIV)
Model: Lichee Pi Nano
DRAM:  32 MiB
MMC:   SUNXI SD/MMC: 0
SF: Detected w25q128bv with page size 256 Bytes, erase size 4 KiB, total 16 MiB
Setting up a 480x272 lcd console (overscan 0x0)
In:    serial@1c25000
Out:   serial@1c25000
Err:   serial@1c25000
Net:   No ethernet found.
starting USB...
No controllers found
Hit any key to stop autoboot:  0 
suda#
```



## kernel

### 安装依赖软件包

`sudo apt install libssl-dev`

### 配置&编译

1. `wget http://nano.lichee.pro/_static/step_by_step/lichee_nano_linux.config` 下载荔枝派nano的默认内核配置文件，`cp lichee_nano_linux.config ./config` 启用默认配置

2. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- menuconfig` 微调配置

   ![设置本地版本号](https://s1.ax1x.com/2018/09/18/iZxeit.png)

   ![选择CPU型号](https://s1.ax1x.com/2018/09/18/iZxnRf.png)

   ![文件系统配置](https://s1.ax1x.com/2018/09/18/iZxYiq.png)

   ![配置MTD设备](https://s1.ax1x.com/2018/10/14/iUt3l9.png)

   ![配置SPI驱动](https://s1.ax1x.com/2018/10/14/iUt8yR.png)

   ![配置I2C驱动](https://s1.ax1x.com/2018/10/14/iUtGO1.png)

3. mtd设备分区（这里选择在设备树中进行设置）

   ```makefile
   &spi0 {
   	pinctrl-names = "default";
   	pinctrl-0 = <&spi0_pins_a>;
   	status = "okay";
   
   	flash: w25q128@0 {
   		#address-cells = <1>;
   		#size-cells = <1>;
   		compatible = "winbond,w25q128", "jedec,spi-nor";
   		reg = <0>;
   		spi-max-frequency = <50000000>;
   		partitions {
               compatible = "fixed-partitions";
               #address-cells = <1>;
               #size-cells = <1>;
               partition@0 {
                   label = "u-boot";
                   reg = <0x000000 0x100000>;
                   read-only;
               };
               partition@100000 {
                   label = "dtb";
                   reg = <0x100000 0x10000>;
                   read-only;
               };
               partition@110000 {
                   label = "kernel";
                   reg = <0x110000 0x400000>;
                   read-only;
               };
               partition@510000 {
                   label = "rootfs";
                   reg = <0x510000 0xAF0000>;
               };
           };
   	};
   };
   ```

4. 制作开机logo

   `jpegtopnm logo-kernel.jpg | pnmquant 224 | pnmtoplainpnm > logo_linux_clut224.ppm`（将生成的ppm文件存放到drivers/video/logo/logo_linux_clut224.ppm)

5. 添加开机logo

   ![开机logo](https://s1.ax1x.com/2018/10/04/i3XPW4.png)

6. make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- -j4 编译

### 烧写kernel到spi-flash中（使用sunxi-tools工具）

```bash
sudo sunxi-fel -p spiflash-write 0x100000 suniv-f1c100s-licheepi-nano-with-lcd.dtb 
100% [================================================]     8 kB,   32.3 kB/s 
sudo sunxi-fel -p spiflash-write 0x110000 zImage 
100% [================================================]  3892 kB,   91.8 kB/s 
```



## rootfs

### 配置&编译

1. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- menuconfig`

   ![Target配置](https://s1.ax1x.com/2018/09/18/ieJusf.png)

   ![Toolchain配置](https://s1.ax1x.com/2018/09/18/ieJcS1.png)

   ![System配置](https://s1.ax1x.com/2018/09/18/ieJoYd.png)

   ![getty设置](https://s1.ax1x.com/2018/09/18/ieJHSI.png)

   ![软件包配置](https://s1.ax1x.com/2018/09/18/ieYoHU.png)

   ![文件系统配置](https://s1.ax1x.com/2018/10/04/i8SIwn.png)

2. 页大小`0x100` 256字节，块大小`0x10000` 64k，jffs2分区总空间`0xAF0000`

3. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi-` 下载源码，编译，打包生成最终的根文件系统

### 烧写rootfs到spi-flash中（使用sunxi-tools工具）

```bash
sudo sunxi-fel -p spiflash-write 0x510000 rootfs.jffs2 
100% [================================================] 11469 kB,  102.2 kB/s 
```




## sunxi-tools安装

1. `sudo apt-get install zlib1g-dev libusb-1.0-0-dev`
2. `make && sudo make install`

### sunxi-tools命令使用

* 1. 如果spi-flash中没有内容，并且没有插入SD卡，那么上电后最后最终会进入fel模式
  2. 如果spi-flash中有内容（不是uboot），则需要在上电前拉低spi-flash的`cs`引脚（一般是1号脚），上电后再松开
  3. 如果spi-flash中已经存在了uboot，可以在上电后进入uboot终端，执行 `sf probe 0;sf erase 0 0x100000;reset`即可重新进入fel模式

* 基本命令使用

1. 查看芯片信息（一般通过该命令查看芯片是否进入了fel模式）

   `sudo sunxi-fel ver`

2. 列出所有芯片的信息

   `sudo sunxi-fel -l`

3. 加载并执行uboot的spl

   `sudo sunxi-fel spl file`

4. 加载并执行uboot

   `sudo sunxi-fel uboot file-with-spl`

5. 把文件内容写入内存指定地址并显示进度

   `sudo sunxi-fel -p write address file`

6. 运行指定地址的函数

   `sudo sunxi-fel exec 地址`

7. 查看spiflash的信息

   `sudo sunxi-fel spiflash-info`

8. 显示spiflash指定地址的数据并写入到文件

   `sudo sunxi-fel spiflash-read addr length file`

9. 写入指定文件的指定长度的内容到spiflash的指定地址

   `sudo sunxi-fel spiflash-write address file`



## 参考文献

[crosstool-NG官方文档](https://crosstool-ng.github.io/docs/)

[荔枝派nano官方文档](http://nano.lichee.pro/)

[sunxi官网](https://linux-sunxi.org/Main_Page)
