# 功能
- 双击鼠标中键关闭标签页
- 保留最后标签页（防止关闭最后一个标签页时关闭浏览器，点X不行）
- 鼠标悬停标签栏滚动
- 按住右键时滚轮滚动标签栏
- 便携设计，程序放在App目录，数据放在Data目录（不兼容原版数据，可以重装系统换电脑不丢数据）
- 移除更新错误警告（因为是绿色版没有自动更新功能）
# 自定义
本dll提供有限的自定义选项（dll版本需要为1.5.7.0 +），新建一个名为config.ini的文件到dll同目录即可。可以使用%app%来表示dll所在目录
```
[dir_setting]
data=%app%\..\Data
cache=%app%\..\Cache
[features]
right_click_close_tab=1
```
# vivaldi 安装包
请访问 https://vivaldi.czyt.tech 获取下载链接
# 获取
采用GitHub Actions自动编译发布，下载地址：[Powered by nightly.link](https://nightly.link/ca-x/vivaldi_plus/workflows/build/main)

[![build status](https://github.com/czyt/vivaldi_plus/actions/workflows/build.yml/badge.svg)](https://github.com/czyt/vivaldi_plus/actions/workflows/build.yml)
# 安装
dll放入解压版Vivaldi目录(vivaldi.exe同目录)即可
## 源项目
[chromePlus](https://github.com/icy37785/chrome_plus)

> 修复 Chrome 118+ 的代码参考了 [Bush2021的项目](https://github.com/Bush2021/chrome_plus)，在此表示感谢。我不会cpp，所以代码写的很烂。版本号更新频繁，看不惯的可以不看。
