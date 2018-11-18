## USB Camera 适配记录

### 内核启用UVC驱动

![内核配置UVC驱动](https://s1.ax1x.com/2018/11/18/FSQ5cQ.png)

### buildroot安装应用软件

![启用fswebcam软件包](https://s1.ax1x.com/2018/11/18/FSQc7t.png)

### fswebcam 使用

```bash
fswebcam -d /dev/video0 --no-banner -r 320x240 capture.jpg
```

