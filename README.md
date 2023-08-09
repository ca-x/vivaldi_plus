# 功能
- 双击关闭标签页
- 保留最后标签页（防止关闭最后一个标签页时关闭浏览器，点X不行）
- 鼠标悬停标签栏滚动
- 按住右键时滚轮滚动标签栏
- 便携设计，程序放在App目录，数据放在Data目录（不兼容原版数据，可以重装系统换电脑不丢数据）
- 移除更新错误警告（因为是绿色版没有自动更新功能）
# 获取
采用GitHub Actions自动编译发布，下载地址：[Powered by nightly.link](https://nightly.link/czyt/vivaldi_plus/workflows/build/main)

[![build status](https://github.com/czyt/vivaldi_plus/actions/workflows/build.yml/badge.svg)](https://github.com/czyt/vivaldi_plus/actions/workflows/build.yml)
# 安装
dll放入解压版Vivaldi目录(vivaldi.exe同目录)即可
## 依赖
本项目需要yaml-cpp库。可以使用以下命令进行安装：
'sudo apt-get install libyaml-cpp-dev'
确保在项目设置中包含库的头文件。
## 源项目
[chromePlus](https://github.com/icy37785/chrome_plus)