# f1c100s sd版教程
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

## SD卡(1GB)分区规划

| 分区序号                | 分区大小 | 分区作用            |
| ----------------------- | -------- | ------------------- |
| (boot区)                | 1MB      | uboot               |
| mmcblk0p1(fat文件系统)  | 16MB     | dtb+boot.scr+kernel |
| mmcblk0p2(ext4文件系统) | 剩余     | 根文件系统          |

![新建内核分区](https://s1.ax1x.com/2018/11/10/iqncrQ.png)

![新建根文件系统分区](https://s1.ax1x.com/2018/11/10/iqnLZ9.png)

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

   ![环境变量配置](https://s1.ax1x.com/2018/11/10/iqQmz6.png)

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

### 启动参数文件boot.cmd

```bash
setenv bootargs console=tty0 console=ttyS0,115200 panic=5 rootwait root=/dev/mmcblk0p2 rw
load mmc 0:1 0x80C00000 suniv-f1c100s-licheepi-nano.dtb
load mmc 0:1 0x80008000 zImage
bootz 0x80008000 - 0x80C00000
```

#### 编译boot.cmd

```bash
mkimage -C none -A arm -T script -d boot.cmd boot.scr
```

### 烧写uboot到SD卡中（使用dd工具）

```bash
sudo dd if=u-boot-sunxi-with-spl.bin of=/dev/sdc bs=1024 seek=8
[sudo] wendy 的密码： 
记录了984+0 的读入
记录了984+0 的写出
1007616 bytes (1.0 MB, 984 KiB) copied, 0.430707 s, 2.3 MB/s
```

## kernel

### 安装依赖软件包

`sudo apt install libssl-dev`

### 配置&编译

1. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- menuconfig` 

   ![设置本地版本号](https://s1.ax1x.com/2018/09/18/iZxeit.png)

   ![选择CPU型号](https://s1.ax1x.com/2018/09/18/iZxnRf.png)

   ![文件系统配置](https://s1.ax1x.com/2018/09/18/iZxYiq.png)

   ![配置MTD设备](https://s1.ax1x.com/2018/10/14/iUt3l9.png)

   ![配置SPI驱动](https://s1.ax1x.com/2018/10/14/iUt8yR.png)

   ![配置I2C驱动](https://s1.ax1x.com/2018/10/14/iUtGO1.png)

2. 制作开机logo

   `jpegtopnm logo-kernel.jpg | pnmquant 224 | pnmtoplainpnm > logo_linux_clut224.ppm`（将生成的ppm文件存放到drivers/video/logo/logo_linux_clut224.ppm)

3. 添加开机logo

   ![开机logo](https://s1.ax1x.com/2018/10/04/i3XPW4.png)

4. make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- -j4 编译

## rootfs

### 配置&编译

1. `make ARCH=arm CROSS_COMPILE=arm-suda-linux-musleabi- menuconfig`

   ![Target配置](https://s1.ax1x.com/2018/09/18/ieJusf.png)

   ![Toolchain配置](https://s1.ax1x.com/2018/09/18/ieJcS1.png)

   ![System配置](https://s1.ax1x.com/2018/09/18/ieJoYd.png)

   ![getty设置](https://s1.ax1x.com/2018/09/18/ieJHSI.png)

   ![软件包配置](https://s1.ax1x.com/2018/09/18/ieYoHU.png)

   ![文件系统配置](https://s1.ax1x.com/2018/10/04/i8SIwn.png)


## 应用开发

### 使用U盘

```bash
1. lsusb
2. ls /dev/sd*
3. mount -t vfat /dev/sda1 /mnt
4. umount /dev/sda1
```

## 参考文献

[crosstool-NG官方文档](https://crosstool-ng.github.io/docs/)

[荔枝派nano官方文档](http://nano.lichee.pro/)

[sunxi官网](https://linux-sunxi.org/Main_Page)
